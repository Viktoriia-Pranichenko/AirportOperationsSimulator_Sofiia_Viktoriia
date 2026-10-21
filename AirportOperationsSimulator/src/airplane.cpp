#include "airplane.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    constexpr Color Fuselage{224, 231, 226, 255};
    constexpr Color FuselageShade{170, 188, 190, 255};
    constexpr Color WindowColor{39, 116, 148, 255};
    constexpr Color EngineColor{55, 66, 72, 255};
    constexpr Color EngineInner{29, 38, 43, 255};
    constexpr Color GearColor{39, 46, 49, 255};

    Color Darken(Color color, float amount)
    {
        return {
            static_cast<unsigned char>(color.r * amount),
            static_cast<unsigned char>(color.g * amount),
            static_cast<unsigned char>(color.b * amount),
            color.a
        };
    }

    Vector3 TransformLocal(Vector3 local, Vector2 position, float cosine, float sine,
        float scale, float pitchRadians, float altitude)
    {
        const float pitchedY = local.y * std::cos(pitchRadians) - local.z * std::sin(pitchRadians);
        const float pitchedZ = local.y * std::sin(pitchRadians) + local.z * std::cos(pitchRadians);
        return {
            position.x + (local.x * cosine - pitchedZ * sine) * scale,
            (pitchedY + 3.5f) * scale + altitude,
            position.y + (local.x * sine + pitchedZ * cosine) * scale
        };
    }

    void Triangle(Vector3 a, Vector3 b, Vector3 c, Vector2 position,
        float cosine, float sine, float scale, float pitchRadians,
        float altitude, Color color)
    {
        DrawTriangle3D(TransformLocal(a, position, cosine, sine, scale, pitchRadians, altitude),
            TransformLocal(b, position, cosine, sine, scale, pitchRadians, altitude),
            TransformLocal(c, position, cosine, sine, scale, pitchRadians, altitude), color);
    }

    void Wing(Vector3 rootFront, Vector3 tipFront, Vector3 tipBack, Vector3 rootBack,
        Vector2 position, float cosine, float sine, float scale,
        float pitchRadians, float altitude, Color wingTop, Color wingBottom)
    {
        const float thickness = 0.7f;
        const Vector3 rootFrontBottom{rootFront.x, rootFront.y - thickness, rootFront.z};
        const Vector3 tipFrontBottom{tipFront.x, tipFront.y - thickness, tipFront.z};
        const Vector3 tipBackBottom{tipBack.x, tipBack.y - thickness, tipBack.z};
        const Vector3 rootBackBottom{rootBack.x, rootBack.y - thickness, rootBack.z};

        Triangle(rootFront, tipFront, tipBack, position, cosine, sine, scale, pitchRadians, altitude, wingTop);
        Triangle(rootFront, tipBack, rootBack, position, cosine, sine, scale, pitchRadians, altitude, wingTop);
        Triangle(rootFrontBottom, tipBackBottom, tipFrontBottom, position, cosine, sine, scale, pitchRadians, altitude, wingBottom);
        Triangle(rootFrontBottom, rootBackBottom, tipBackBottom, position, cosine, sine, scale, pitchRadians, altitude, wingBottom);
        Triangle(rootFront, rootFrontBottom, tipFrontBottom, position, cosine, sine, scale, pitchRadians, altitude, FuselageShade);
        Triangle(rootFront, tipFrontBottom, tipFront, position, cosine, sine, scale, pitchRadians, altitude, FuselageShade);
        Triangle(rootBack, tipBack, tipBackBottom, position, cosine, sine, scale, pitchRadians, altitude, wingBottom);
        Triangle(rootBack, tipBackBottom, rootBackBottom, position, cosine, sine, scale, pitchRadians, altitude, wingBottom);
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
        allowed = nextState == AirplaneState::AwaitingTaxiAuthorization ||
            nextState == AirplaneState::TaxiingToStand;
        break;
    case AirplaneState::AwaitingTaxiAuthorization:
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
        allowed = nextState == AirplaneState::AwaitingTakeoffAuthorization ||
            nextState == AirplaneState::TakingOff || nextState == AirplaneState::Delayed;
        break;
    case AirplaneState::AwaitingTakeoffAuthorization:
        allowed = nextState == AirplaneState::TaxiingToRunway;
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
    AirplaneState completionState, int transitionIndex, AirplaneState intermediateState,
    int touchdownWaypoint)
{
    route = std::move(newRoute);
    nextWaypoint = route.empty() ? 0 : 1; // route[0] is the current location
    routeSpeed = std::max(0.0f, newRouteSpeed);
    routeCompletionState = completionState;
    transitionWaypointIndex = transitionIndex;
    routeIntermediateState = intermediateState;
    intermediateTransitionDone = false;
    speed = 0.0f;
    hasTarget = !route.empty();
    if (hasTarget) targetPosition = route.back();
    if (state == AirplaneState::Landing && route.size() > 1)
    {
        landingApproachLength = 0.0f;
        landingTouchdownWaypoint = touchdownWaypoint >= 1 ?
            std::min<std::size_t>(static_cast<std::size_t>(touchdownWaypoint), route.size() - 1) :
            std::min<std::size_t>(4, route.size() - 1);
        for (std::size_t i = 1; i <= landingTouchdownWaypoint; ++i)
        {
            const float dx = route[i].x - route[i - 1].x;
            const float dz = route[i].y - route[i - 1].y;
            landingApproachLength += std::sqrt(dx * dx + dz * dz);
        }
        altitude = 32.0f;
        pitchDegrees = -5.0f;
    }
    else
    {
        landingApproachLength = 0.0f;
        landingTouchdownWaypoint = 0;
    }
}

void Airplane::Update(float deltaTime)
{
    if (deltaTime > 0.0f) animationTime += deltaTime;
    if (deltaTime <= 0.0f || !hasTarget || route.empty()) return;

    float distanceToEnd = 0.0f;
    Vector2 cursor = position;
    for (std::size_t i = nextWaypoint; i < route.size(); ++i)
    {
        const float dx = route[i].x - cursor.x;
        const float dz = route[i].y - cursor.y;
        distanceToEnd += std::sqrt(dx * dx + dz * dz);
        cursor = route[i];
    }

    const float deceleration = state == AirplaneState::Landing ? 7.0f : 5.0f;
    float desiredSpeed = routeSpeed;
    if (state == AirplaneState::TaxiingToStand)
        desiredSpeed = std::min(desiredSpeed, 9.0f);
    if (state == AirplaneState::TaxiingToRunway)
        desiredSpeed = std::min(desiredSpeed, 11.0f);
    // Every routed movement ends at a hold point or a stand. Brake smoothly
    // using the remaining path length so the aircraft reaches it at rest.
    if (state != AirplaneState::TakingOff)
        desiredSpeed = std::min(desiredSpeed,
            std::sqrt(std::max(0.0f, 2.0f * deceleration * distanceToEnd)));
    const float acceleration = state == AirplaneState::TakingOff ? 10.0f : 4.0f;
    const float speedChange = std::clamp(desiredSpeed - speed,
        -deceleration * deltaTime, acceleration * deltaTime);
    speed = std::max(0.0f, speed + speedChange);
    const float step = std::min(speed * deltaTime, distanceToEnd);

    float remainingStep = step;
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
            headingDegrees = std::remainder(headingDegrees +
                std::clamp(headingDelta, -turnStep, turnStep), 360.0f);
            if (remainingStep < distance)
            {
                const float ratio = remainingStep / distance;
                position.x += dx * ratio;
                position.y += dz * ratio;
                remainingStep = 0.0f;
                break;
            }
            position = waypoint;
            remainingStep -= distance;
        }

        if (static_cast<int>(nextWaypoint) == transitionWaypointIndex && !intermediateTransitionDone)
        {
            TransitionTo(routeIntermediateState);
            intermediateTransitionDone = true;
        }
        ++nextWaypoint;
        if (remainingStep <= 0.0f) break;
    }

    if (state == AirplaneState::Landing && landingApproachLength > 0.0f &&
        nextWaypoint <= landingTouchdownWaypoint)
    {
        float remainingApproach = 0.0f;
        Vector2 cursor = position;
        for (std::size_t i = nextWaypoint; i <= landingTouchdownWaypoint; ++i)
        {
            const float dx = route[i].x - cursor.x;
            const float dz = route[i].y - cursor.y;
            remainingApproach += std::sqrt(dx * dx + dz * dz);
            cursor = route[i];
        }
        altitude = std::clamp(remainingApproach / landingApproachLength, 0.0f, 1.0f) * 32.0f;
        pitchDegrees = -5.0f * std::clamp(altitude / 32.0f, 0.0f, 1.0f);
    }
    else if (state == AirplaneState::Landing)
    {
        altitude = 0.0f;
        pitchDegrees = 0.0f;
    }

    if (state == AirplaneState::TakingOff)
    {
        float totalDistance = 0.0f;
        for (std::size_t i = 1; i < route.size(); ++i)
        {
            const float dx = route[i].x - route[i - 1].x;
            const float dz = route[i].y - route[i - 1].y;
            totalDistance += std::sqrt(dx * dx + dz * dz);
        }
        const float progress = totalDistance > 0.0f ?
            std::clamp(1.0f - distanceToEnd / totalDistance, 0.0f, 1.0f) : 0.0f;
        const float lift = std::clamp((progress - 0.45f) / 0.55f, 0.0f, 1.0f);
        pitchDegrees = 15.0f * lift;
        altitude = 30.0f * lift;
    }

    if (nextWaypoint >= route.size())
    {
        hasTarget = false;
        speed = 0.0f;
        route.clear();
        TransitionTo(routeCompletionState);
        if (state != AirplaneState::TakingOff)
        {
            altitude = 0.0f;
            pitchDegrees = 0.0f;
        }
    }
}

void Airplane::Draw(float scale) const
{
    DrawPose(position, headingDegrees, scale, pitchDegrees, altitude);
}

void Airplane::Draw(Vector2 drawPosition, float drawHeadingDegrees, float scale) const
{
    DrawPose(drawPosition, drawHeadingDegrees, scale, 0.0f, 0.0f);
}

void Airplane::DrawPose(Vector2 drawPosition, float drawHeadingDegrees, float scale,
    float drawPitchDegrees, float drawAltitude) const
{
    const float radians = drawHeadingDegrees * DEG2RAD;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float pitchRadians = drawPitchDegrees * DEG2RAD;
    const Color wingTop = liveryColor;
    const Color wingBottom = Darken(liveryColor, 0.62f);
    const float movementBlend = std::clamp(speed / 3.0f, 0.0f, 1.0f);
    // Wing movement follows actual aircraft motion. It fades in while taxiing
    // or flying and settles to neutral as speed reaches zero.
    const float wingFlex = std::sin(animationTime * 2.8f) * 1.6f * movementBlend;
    float gearDeployment = 1.0f;
    if (state == AirplaneState::TakingOff)
    {
        const float retract = std::clamp((drawAltitude - 2.0f) / 24.0f, 0.0f, 1.0f);
        gearDeployment = 1.0f - retract;
    }
    float flapDeflectionDegrees = 0.0f;
    if ((state == AirplaneState::Landing || state == AirplaneState::Arriving) && drawAltitude > 1.0f)
        flapDeflectionDegrees = -4.0f * movementBlend;
    else if (state == AirplaneState::TakingOff)
        flapDeflectionDegrees = -3.0f *
            (1.0f - std::clamp(drawAltitude / 30.0f, 0.0f, 1.0f)) * movementBlend;

    // A soft ground shadow makes the flare, touchdown, and initial climb easy
    // to read against the runway from the game's elevated camera.
    const float shadowAlpha = std::clamp(150.0f - drawAltitude * 2.0f, 35.0f, 150.0f);
    const unsigned char alpha = static_cast<unsigned char>(shadowAlpha);
    DrawCircle3D({drawPosition.x + drawAltitude * 0.08f, 0.13f,
        drawPosition.y + drawAltitude * 0.05f},
        (14.0f + drawAltitude * 0.18f) * scale, {1.0f, 0.0f, 0.0f}, 90.0f,
        Color{14, 23, 27, alpha});
    const auto transform = [&](Vector3 local)
    {
        return TransformLocal(local, drawPosition, cosine, sine, scale,
            pitchRadians, drawAltitude);
    };

    // The broad wings and tailplane are thin 3D prisms with shaded undersides.
    Wing({-3.5f, 1.2f, -9.0f}, {-44.0f, 0.5f + wingFlex, 12.0f},
         {-39.0f, 0.5f + wingFlex, 20.0f}, {-3.5f, 1.2f, 4.0f},
         drawPosition, cosine, sine, scale, pitchRadians, drawAltitude, wingTop, wingBottom);
    Wing({3.5f, 1.2f, -9.0f}, {44.0f, 0.5f + wingFlex, 12.0f},
         {39.0f, 0.5f + wingFlex, 20.0f}, {3.5f, 1.2f, 4.0f},
         drawPosition, cosine, sine, scale, pitchRadians, drawAltitude, wingTop, wingBottom);

    // Animated control-surface panels along each wing's trailing edge.
    const auto drawFlap = [&](Vector3 hingeInner, Vector3 hingeOuter,
        Vector3 trailingInner, Vector3 trailingOuter)
    {
        const float deflection = flapDeflectionDegrees * DEG2RAD;
        const auto lowerEdge = [deflection](Vector3 point, Vector3 hinge)
        {
            const float chord = point.z - hinge.z;
            point.y = hinge.y + std::sin(deflection) * chord + 0.06f;
            point.z = hinge.z + std::cos(deflection) * chord;
            return point;
        };
        trailingInner = lowerEdge(trailingInner, hingeInner);
        trailingOuter = lowerEdge(trailingOuter, hingeOuter);
        const Color flapColor = Darken(liveryColor, 0.72f);
        Triangle(hingeInner, hingeOuter, trailingOuter, drawPosition, cosine, sine,
            scale, pitchRadians, drawAltitude, flapColor);
        Triangle(hingeInner, trailingOuter, trailingInner, drawPosition, cosine, sine,
            scale, pitchRadians, drawAltitude, flapColor);
        DrawLine3D(transform(hingeInner), transform(trailingInner), FuselageShade);
        DrawLine3D(transform(hingeOuter), transform(trailingOuter), FuselageShade);
    };
    drawFlap({-14.0f, 1.0f, 4.0f}, {-34.0f, 0.67f, 8.0f},
        {-14.0f, 0.9f, 11.0f}, {-34.0f, 0.58f, 16.5f});
    drawFlap({14.0f, 1.0f, 4.0f}, {34.0f, 0.67f, 8.0f},
        {14.0f, 0.9f, 11.0f}, {34.0f, 0.58f, 16.5f});
    Wing({-2.5f, 1.1f, 25.0f}, {-20.0f, 0.5f, 34.0f},
         {-17.0f, 0.5f, 38.0f}, {-2.5f, 1.1f, 32.0f},
         drawPosition, cosine, sine, scale, pitchRadians, drawAltitude, wingTop, wingBottom);
    Wing({2.5f, 1.1f, 25.0f}, {20.0f, 0.5f, 34.0f},
         {17.0f, 0.5f, 38.0f}, {2.5f, 1.1f, 32.0f},
         drawPosition, cosine, sine, scale, pitchRadians, drawAltitude, wingTop, wingBottom);

    // Rounded fuselage, tapered toward the nose and tail.
    DrawCylinderEx(transform({0.0f, 0.9f, -25.0f}),
        transform({0.0f, 1.0f, 22.0f}),
        2.5f * scale, 2.3f * scale, 12, Fuselage);
    DrawCylinderEx(transform({0.0f, 0.9f, -25.0f}),
        transform({0.0f, 0.9f, -42.0f}),
        2.5f * scale, 0.15f * scale, 12, Fuselage);
    DrawCylinderEx(transform({0.0f, 1.0f, 22.0f}),
        transform({0.0f, 0.8f, 38.0f}),
        2.3f * scale, 0.12f * scale, 12, FuselageShade);

    // Tall vertical fin gives the tail a clear 3D profile.
    Triangle({-2.0f, 1.2f, 25.0f}, {0.0f, 9.0f, 36.0f}, {0.0f, 1.2f, 36.0f},
        drawPosition, cosine, sine, scale, pitchRadians, drawAltitude, wingTop);
    Triangle({2.0f, 1.2f, 25.0f}, {0.0f, 1.2f, 36.0f}, {0.0f, 9.0f, 36.0f},
        drawPosition, cosine, sine, scale, pitchRadians, drawAltitude, wingBottom);
    Triangle({0.0f, 9.0f, 36.0f}, {0.0f, 1.2f, 36.0f}, {0.0f, 1.2f, 25.0f},
        drawPosition, cosine, sine, scale, pitchRadians, drawAltitude, FuselageShade);

    // Engine pods and dark inlets beneath the main wings.
    const float sides[] = {-1.0f, 1.0f};
    for (const float side : sides)
    {
        const float x = side * 22.0f;
        DrawCylinderEx(transform({x, 0.0f, 8.0f}),
            transform({x, 0.0f, 19.0f}),
            3.2f * scale, 2.6f * scale, 10, EngineColor);
        DrawSphereEx(transform({x, 0.0f, 7.8f}),
            2.3f * scale, 6, 8, EngineInner);
    }

    // Blue cockpit glazing, a row of cabin windows, and the tail accent.
    DrawSphereEx(transform({0.0f, 2.1f, -29.0f}),
        1.8f * scale, 8, 10, WindowColor);
    for (int window = 0; window < 7; ++window)
    {
        const float z = -19.0f + window * 4.5f;
        for (const float side : sides)
            DrawSphereEx(transform({side * 2.0f, 2.45f, z}),
                0.42f * scale, 5, 6, WindowColor);
    }
    DrawCylinderEx(transform({0.0f, 2.15f, 17.0f}),
        transform({0.0f, 2.15f, 22.0f}),
        0.55f * scale, 0.42f * scale, 8, liveryColor);

    // Simple landing gear makes the plane feel grounded on the airport surface.
    for (const float side : sides)
    {
        const Vector3 strutTop{side * 3.0f, 0.2f + 0.9f * (1.0f - gearDeployment), 4.0f};
        const Vector3 wheelPosition{side * 6.0f, -2.6f + 4.1f * (1.0f - gearDeployment), 4.0f};
        DrawCylinderEx(transform(strutTop),
            transform(wheelPosition),
            0.28f * scale, 0.28f * scale, 6, GearColor);
        DrawSphereEx(transform(wheelPosition),
            0.8f * scale, 6, 8, GearColor);
    }
}
