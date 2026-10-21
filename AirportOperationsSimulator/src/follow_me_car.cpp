#include "follow_me_car.h"

#include "airport_terminal.h"
#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace
{
    constexpr Color FollowYellow{226, 181, 48, 255};
    constexpr Color FollowBlack{39, 43, 43, 255};
    constexpr Color Glass{42, 100, 125, 255};
    constexpr Color Trim{195, 201, 192, 255};
    constexpr Color Tire{37, 42, 43, 255};
    constexpr Color WheelHub{162, 166, 155, 255};
    constexpr float DegreesToRadians = PI / 180.0f;

    void DrawWheel(float x, float y, float z, float radius)
    {
        DrawCylinderEx({x - 0.14f, y, z}, {x + 0.14f, y, z},
            radius, radius, 10, Tire);
        DrawCylinderEx({x - 0.16f, y, z}, {x - 0.07f, y, z},
            radius * 0.46f, radius * 0.46f, 8, WheelHub);
    }
}

FollowMeCar::FollowMeCar(float positionX, float positionZ)
    : ServiceVehiclePosition(positionX, positionZ)
{
}

bool FollowMeCar::MoveTowardsGate(
    int gateNumber, const AirportTerminal& terminal, float deltaTime)
{
    if (gateNumber < 1 || deltaTime <= 0.0f) return false;

    if (route.GetGateNumber() != gateNumber || route.GetWaypoints().empty())
    {
        if (!route.BuildToEastWingGate(gateNumber, terminal,
            {positionX, positionZ}, headingDegrees))
            return false;
        nextRoutePoint = 1;
    }

    constexpr float MovementSpeed = 12.0f;
    constexpr float MaximumSteeringRate = 75.0f * DegreesToRadians;
    const std::vector<Vector2>& waypoints = route.GetWaypoints();
    float remainingTime = deltaTime;
    while (nextRoutePoint < waypoints.size() && remainingTime > 0.0f)
    {
        const Vector2 waypoint = waypoints[nextRoutePoint];
        const float dx = waypoint.x - positionX;
        const float dz = waypoint.y - positionZ;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance <= 0.001f)
        {
            ++nextRoutePoint;
            continue;
        }

        const float pathHeading = std::atan2(dx, dz) / DegreesToRadians;
        const bool reversing =
            nextRoutePoint <= route.GetReverseWaypointCount();
        headingDegrees = std::remainder(
            pathHeading + (reversing ? 180.0f : 0.0f), 360.0f);

        float segmentSpeed = MovementSpeed;
        if (nextRoutePoint + 1 < waypoints.size())
        {
            const Vector2 nextWaypoint = waypoints[nextRoutePoint + 1];
            const float nextHeading = std::atan2(
                nextWaypoint.x - waypoint.x, nextWaypoint.y - waypoint.y);
            const float segmentHeading = std::atan2(dx, dz);
            const float turnAngle = std::abs(std::remainder(
                nextHeading - segmentHeading, 2.0f * PI));
            if (turnAngle > 0.001f)
            {
                const Vector2 previousWaypoint = waypoints[nextRoutePoint - 1];
                const float segmentDx = waypoint.x - previousWaypoint.x;
                const float segmentDz = waypoint.y - previousWaypoint.y;
                const float segmentLength = std::sqrt(
                    segmentDx * segmentDx + segmentDz * segmentDz);
                const float turnRadius = segmentLength / turnAngle;
                segmentSpeed = std::min(segmentSpeed,
                    turnRadius * MaximumSteeringRate);
            }
        }

        const float travelDistance = std::min(distance,
            segmentSpeed * remainingTime);
        positionX += dx / distance * travelDistance;
        positionZ += dz / distance * travelDistance;
        remainingTime -= travelDistance / segmentSpeed;
        if (travelDistance < distance) break;
        ++nextRoutePoint;
    }

    return nextRoutePoint >= waypoints.size();
}

const ServiceVehicleRoute& FollowMeCar::GetRoute() const
{
    return route;
}

void FollowMeCar::Draw() const
{
    rlPushMatrix();
    rlTranslatef(positionX, 0.0f, positionZ);
    rlRotatef(headingDegrees, 0.0f, 1.0f, 0.0f);

    DrawCubeV({0.0f, 0.72f, 0.0f}, {2.08f, 0.48f, 4.55f}, FollowBlack);
    DrawCubeV({0.0f, 1.08f, 0.12f}, {2.0f, 0.78f, 3.58f}, FollowYellow);
    DrawCubeV({0.0f, 1.53f, -0.35f}, {1.84f, 0.86f, 1.82f}, FollowYellow);
    DrawCubeV({0.0f, 1.98f, -0.35f}, {1.9f, 0.12f, 1.9f}, FollowBlack);

    DrawCubeV({0.0f, 1.57f, 0.59f}, {1.5f, 0.55f, 0.05f}, Glass);
    DrawCubeV({-0.94f, 1.5f, -0.35f}, {0.05f, 0.5f, 0.94f}, Glass);
    DrawCubeV({0.94f, 1.5f, -0.35f}, {0.05f, 0.5f, 0.94f}, Glass);

    for (float side : {-1.0f, 1.0f})
    {
        for (float stripeZ : {-1.65f, -0.85f, 0.0f, 0.85f, 1.65f})
        {
            DrawCubeV({side * 1.015f, 0.99f, stripeZ},
                {0.035f, 0.28f, 0.46f}, FollowBlack);
        }

        for (float axleZ : {-1.48f, 1.48f})
            DrawWheel(side * 1.04f, 0.38f, axleZ, 0.38f);
    }

    DrawCubeV({0.0f, 2.12f, -0.35f}, {0.32f, 0.08f, 0.3f}, FollowYellow);
    DrawCubeV({0.0f, 2.27f, -0.35f}, {0.24f, 0.2f, 0.22f},
        Color{220, 52, 37, 255});
    DrawCubeV({-0.68f, 1.14f, 2.34f}, {0.32f, 0.16f, 0.06f}, Trim);
    DrawCubeV({0.68f, 1.14f, 2.34f}, {0.32f, 0.16f, 0.06f}, Trim);
    rlPopMatrix();
}
