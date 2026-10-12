#include "airplane.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    constexpr Color WingTop{48, 125, 160, 255};
    constexpr Color WingBottom{31, 82, 108, 255};
    constexpr Color Fuselage{224, 231, 226, 255};
    constexpr Color FuselageShade{170, 188, 190, 255};
    constexpr Color WindowColor{39, 116, 148, 255};
    constexpr Color EngineColor{55, 66, 72, 255};
    constexpr Color EngineInner{29, 38, 43, 255};
    constexpr Color AccentColor{224, 151, 57, 255};
    constexpr Color GearColor{39, 46, 49, 255};

    Vector3 TransformLocal(Vector3 local, Vector2 position, float cosine, float sine, float scale)
    {
        return {
            position.x + (local.x * cosine - local.z * sine) * scale,
            (local.y + 3.5f) * scale,
            position.y + (local.x * sine + local.z * cosine) * scale
        };
    }

    void Triangle(Vector3 a, Vector3 b, Vector3 c, Vector2 position,
        float cosine, float sine, float scale, Color color)
    {
        DrawTriangle3D(TransformLocal(a, position, cosine, sine, scale),
            TransformLocal(b, position, cosine, sine, scale),
            TransformLocal(c, position, cosine, sine, scale), color);
    }

    void Wing(Vector3 rootFront, Vector3 tipFront, Vector3 tipBack, Vector3 rootBack,
        Vector2 position, float cosine, float sine, float scale)
    {
        const float thickness = 0.7f;
        const Vector3 rootFrontBottom{rootFront.x, rootFront.y - thickness, rootFront.z};
        const Vector3 tipFrontBottom{tipFront.x, tipFront.y - thickness, tipFront.z};
        const Vector3 tipBackBottom{tipBack.x, tipBack.y - thickness, tipBack.z};
        const Vector3 rootBackBottom{rootBack.x, rootBack.y - thickness, rootBack.z};

        Triangle(rootFront, tipFront, tipBack, position, cosine, sine, scale, WingTop);
        Triangle(rootFront, tipBack, rootBack, position, cosine, sine, scale, WingTop);
        Triangle(rootFrontBottom, tipBackBottom, tipFrontBottom, position, cosine, sine, scale, WingBottom);
        Triangle(rootFrontBottom, rootBackBottom, tipBackBottom, position, cosine, sine, scale, WingBottom);
        Triangle(rootFront, rootFrontBottom, tipFrontBottom, position, cosine, sine, scale, FuselageShade);
        Triangle(rootFront, tipFrontBottom, tipFront, position, cosine, sine, scale, FuselageShade);
        Triangle(rootBack, tipBack, tipBackBottom, position, cosine, sine, scale, WingBottom);
        Triangle(rootBack, tipBackBottom, rootBackBottom, position, cosine, sine, scale, WingBottom);
    }
}

bool Airplane::TransitionTo(AirplaneState nextState)
{
    bool allowed = false;

    switch (state)
    {
    case AirplaneState::Arriving:
        allowed = nextState == AirplaneState::Landing || nextState == AirplaneState::Holding;
        break;
    case AirplaneState::Holding:
        allowed = nextState == AirplaneState::Arriving || nextState == AirplaneState::Landing;
        break;
    case AirplaneState::Landing:
        allowed = nextState == AirplaneState::TaxiingToStand;
        break;
    case AirplaneState::TaxiingToStand:
        allowed = nextState == AirplaneState::Parked;
        break;
    case AirplaneState::Parked:
        allowed = nextState == AirplaneState::PreparingForDeparture;
        break;
    case AirplaneState::PreparingForDeparture:
        allowed = nextState == AirplaneState::TaxiingToRunway || nextState == AirplaneState::Delayed;
        break;
    case AirplaneState::Delayed:
        allowed = nextState == AirplaneState::PreparingForDeparture;
        break;
    case AirplaneState::TaxiingToRunway:
        allowed = nextState == AirplaneState::TakingOff || nextState == AirplaneState::Delayed;
        break;
    case AirplaneState::TakingOff:
        allowed = nextState == AirplaneState::Departed;
        break;
    case AirplaneState::Departed:
    case AirplaneState::Incident:
        allowed = false;
        break;
    }

    if (nextState == AirplaneState::Incident && state != AirplaneState::Departed)
        allowed = true;
    if (!allowed) return false;

    state = nextState;
    return true;
}

void Airplane::SetWaypointRoute(std::vector<Vector2> newRoute, float newRouteSpeed,
    AirplaneState completionState, int transitionIndex, AirplaneState intermediateState)
{
    route = std::move(newRoute);
    nextWaypoint = route.empty() ? 0 : 1; // route[0] is the current location
    routeSpeed = std::max(0.0f, newRouteSpeed);
    routeCompletionState = completionState;
    transitionWaypointIndex = transitionIndex;
    routeIntermediateState = intermediateState;
    intermediateTransitionDone = false;
    speed = routeSpeed;
    hasTarget = !route.empty();
    if (hasTarget) targetPosition = route.back();
    else speed = 0.0f;
}

void Airplane::Update(float deltaTime)
{
    if (!hasTarget || route.empty() || deltaTime <= 0.0f) return;

    float remainingStep = routeSpeed * deltaTime;
    while (nextWaypoint < route.size())
    {
        const Vector2 waypoint = route[nextWaypoint];
        const float dx = waypoint.x - position.x;
        const float dz = waypoint.y - position.y;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance > 0.001f)
        {
            const float desiredHeading = std::atan2(dx, -dz) * RAD2DEG;
            const float headingDelta = std::remainder(desiredHeading - headingDegrees, 360.0f);
            const float turnStep = turnRateDegreesPerSecond * deltaTime;
            headingDegrees += std::clamp(headingDelta, -turnStep, turnStep);
            if (remainingStep < distance)
            {
                const float ratio = remainingStep / distance;
                position.x += dx * ratio;
                position.y += dz * ratio;
                return;
            }
            remainingStep -= distance;
            position = waypoint;
        }

        if (static_cast<int>(nextWaypoint) == transitionWaypointIndex && !intermediateTransitionDone)
        {
            TransitionTo(routeIntermediateState);
            intermediateTransitionDone = true;
        }
        ++nextWaypoint;
        if (remainingStep <= 0.0f && nextWaypoint < route.size()) return;
    }

    hasTarget = false;
    speed = 0.0f;
    route.clear();
    TransitionTo(routeCompletionState);
}

void Airplane::Draw(float scale) const
{
    Draw(position, headingDegrees, scale);
}

void Airplane::Draw(Vector2 drawPosition, float drawHeadingDegrees, float scale) const
{
    const float radians = drawHeadingDegrees * DEG2RAD;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    // The broad wings and tailplane are thin 3D prisms with shaded undersides.
    Wing({-3.5f, 1.2f, -9.0f}, {-44.0f, 0.5f, 12.0f},
         {-39.0f, 0.5f, 20.0f}, {-3.5f, 1.2f, 4.0f},
         drawPosition, cosine, sine, scale);
    Wing({3.5f, 1.2f, -9.0f}, {44.0f, 0.5f, 12.0f},
         {39.0f, 0.5f, 20.0f}, {3.5f, 1.2f, 4.0f},
         drawPosition, cosine, sine, scale);
    Wing({-2.5f, 1.1f, 25.0f}, {-20.0f, 0.5f, 34.0f},
         {-17.0f, 0.5f, 38.0f}, {-2.5f, 1.1f, 32.0f},
         drawPosition, cosine, sine, scale);
    Wing({2.5f, 1.1f, 25.0f}, {20.0f, 0.5f, 34.0f},
         {17.0f, 0.5f, 38.0f}, {2.5f, 1.1f, 32.0f},
         drawPosition, cosine, sine, scale);

    // Rounded fuselage, tapered toward the nose and tail.
    DrawCylinderEx(TransformLocal({0.0f, 0.9f, -25.0f}, drawPosition, cosine, sine, scale),
        TransformLocal({0.0f, 1.0f, 22.0f}, drawPosition, cosine, sine, scale),
        2.5f * scale, 2.3f * scale, 12, Fuselage);
    DrawCylinderEx(TransformLocal({0.0f, 0.9f, -25.0f}, drawPosition, cosine, sine, scale),
        TransformLocal({0.0f, 0.9f, -42.0f}, drawPosition, cosine, sine, scale),
        2.5f * scale, 0.15f * scale, 12, Fuselage);
    DrawCylinderEx(TransformLocal({0.0f, 1.0f, 22.0f}, drawPosition, cosine, sine, scale),
        TransformLocal({0.0f, 0.8f, 38.0f}, drawPosition, cosine, sine, scale),
        2.3f * scale, 0.12f * scale, 12, FuselageShade);

    // Tall vertical fin gives the tail a clear 3D profile.
    Triangle({-2.0f, 1.2f, 25.0f}, {0.0f, 9.0f, 36.0f}, {0.0f, 1.2f, 36.0f},
        drawPosition, cosine, sine, scale, WingTop);
    Triangle({2.0f, 1.2f, 25.0f}, {0.0f, 1.2f, 36.0f}, {0.0f, 9.0f, 36.0f},
        drawPosition, cosine, sine, scale, WingBottom);
    Triangle({0.0f, 9.0f, 36.0f}, {0.0f, 1.2f, 36.0f}, {0.0f, 1.2f, 25.0f},
        drawPosition, cosine, sine, scale, FuselageShade);

    // Engine pods and dark inlets beneath the main wings.
    const float sides[] = {-1.0f, 1.0f};
    for (const float side : sides)
    {
        const float x = side * 22.0f;
        DrawCylinderEx(TransformLocal({x, 0.0f, 8.0f}, drawPosition, cosine, sine, scale),
            TransformLocal({x, 0.0f, 19.0f}, drawPosition, cosine, sine, scale),
            3.2f * scale, 2.6f * scale, 10, EngineColor);
        DrawSphereEx(TransformLocal({x, 0.0f, 7.8f}, drawPosition, cosine, sine, scale),
            2.3f * scale, 6, 8, EngineInner);
    }

    // Blue cockpit glazing, a row of cabin windows, and the tail accent.
    DrawSphereEx(TransformLocal({0.0f, 2.1f, -29.0f}, drawPosition, cosine, sine, scale),
        1.8f * scale, 8, 10, WindowColor);
    for (int window = 0; window < 7; ++window)
    {
        const float z = -19.0f + window * 4.5f;
        for (const float side : sides)
            DrawSphereEx(TransformLocal({side * 2.0f, 2.45f, z}, drawPosition, cosine, sine, scale),
                0.42f * scale, 5, 6, WindowColor);
    }
    DrawCylinderEx(TransformLocal({0.0f, 2.15f, 17.0f}, drawPosition, cosine, sine, scale),
        TransformLocal({0.0f, 2.15f, 22.0f}, drawPosition, cosine, sine, scale),
        0.55f * scale, 0.42f * scale, 8, AccentColor);

    // Simple landing gear makes the plane feel grounded on the airport surface.
    for (const float side : sides)
    {
        const Vector3 strutTop{side * 3.0f, 0.2f, 4.0f};
        const Vector3 wheelPosition{side * 6.0f, -2.6f, 4.0f};
        DrawCylinderEx(TransformLocal(strutTop, drawPosition, cosine, sine, scale),
            TransformLocal(wheelPosition, drawPosition, cosine, sine, scale),
            0.28f * scale, 0.28f * scale, 6, GearColor);
        DrawSphereEx(TransformLocal(wheelPosition, drawPosition, cosine, sine, scale),
            0.8f * scale, 6, 8, GearColor);
    }
}
