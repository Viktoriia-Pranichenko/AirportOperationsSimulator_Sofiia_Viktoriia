#include "service_vehicle_route.h"

#include "airport_terminal.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    constexpr float DegreesToRadians = PI / 180.0f;

    void AppendRouteLine(std::vector<Vector2>& route, float headingDegrees,
        float distance)
    {
        if (route.empty() || distance <= 0.0f) return;
        constexpr float MaximumStep = 1.0f;
        const Vector2 start = route.back();
        const float radians = headingDegrees * DegreesToRadians;
        const float directionX = std::sin(radians);
        const float directionZ = std::cos(radians);
        const int steps = static_cast<int>(std::ceil(distance / MaximumStep));
        for (int step = 1; step <= steps; ++step)
        {
            const float travelled = distance * static_cast<float>(step) / steps;
            route.push_back({start.x + directionX * travelled,
                start.y + directionZ * travelled});
        }
    }

    void AppendRouteTurn(std::vector<Vector2>& route, float& headingDegrees,
        float radius, int turnDirection)
    {
        if (route.empty() || radius <= 0.0f) return;
        constexpr float AngleStepDegrees = 5.0f;
        const float startHeading = headingDegrees * DegreesToRadians;
        const float turnSign = turnDirection < 0 ? -1.0f : 1.0f;
        const Vector2 start = route.back();
        const int steps = static_cast<int>(90.0f / AngleStepDegrees);
        for (int step = 1; step <= steps; ++step)
        {
            const float angle = (90.0f * static_cast<float>(step) / steps) *
                DegreesToRadians;
            const float heading = startHeading + turnSign * angle;
            route.push_back({start.x + radius / turnSign *
                    (std::cos(startHeading) - std::cos(heading)),
                start.y + radius / turnSign *
                    (std::sin(heading) - std::sin(startHeading))});
        }
        headingDegrees += turnSign * 90.0f;
    }
}

bool ServiceVehicleRoute::BuildToEastWingGate(int targetGateNumber,
    const AirportTerminal& terminal, Vector2 startPosition,
    float startHeadingDegrees)
{
    waypoints.clear();
    reverseWaypointCount = 0;
    gateNumber = -1;
    if (targetGateNumber < 1) return false;

    const std::string gateId = "EAST_" + std::to_string(targetGateNumber);
    const AirportParkingStand* targetGate = nullptr;
    for (const AirportParkingStand& stand : terminal.GetParkingStands())
    {
        if (stand.id == gateId)
        {
            targetGate = &stand;
            break;
        }
    }
    if (targetGate == nullptr) return false;

    waypoints.push_back(startPosition);
    float routeHeading = startHeadingDegrees;

    // Reverse into the aisle, then roll forward and turn toward screen right.
    // In the overview camera that is the parking lot's -X end.
    AppendRouteLine(waypoints, routeHeading + 180.0f, 21.25f);
    reverseWaypointCount = waypoints.size() - 1;
    AppendRouteLine(waypoints, routeHeading, 4.0f);
    AppendRouteTurn(waypoints, routeHeading, 4.0f, -1);

    // Leave through the screen-right end, then go around the east side of the
    // terminal and below its south end.
    constexpr float ParkingLotRightExitX = 150.0f;
    constexpr float SouthOfTerminalZ = -220.0f;
    AppendRouteLine(waypoints, routeHeading,
        waypoints.back().x - ParkingLotRightExitX);
    AppendRouteTurn(waypoints, routeHeading, 12.0f, -1);
    AppendRouteLine(waypoints, routeHeading,
        waypoints.back().y - SouthOfTerminalZ);
    AppendRouteTurn(waypoints, routeHeading, 12.0f, 1);

    // Travel west only below the terminal's south end, then turn north into
    // the east wing gate apron.
    AppendRouteLine(waypoints, routeHeading,
        waypoints.back().x - (targetGate->position.x + 10.0f));
    AppendRouteTurn(waypoints, routeHeading, 10.0f, 1);
    const float stoppingZ = targetGate->position.z - 14.0f;
    AppendRouteLine(waypoints, routeHeading, stoppingZ - waypoints.back().y);

    gateNumber = targetGateNumber;
    return true;
}

bool ServiceVehicleRoute::BuildToEastWingGateFromBaggageParking(
    int targetGateNumber, const AirportTerminal& terminal,
    Vector2 startPosition, float startHeadingDegrees)
{
    waypoints.clear();
    reverseWaypointCount = 0;
    gateNumber = -1;
    if (targetGateNumber < 1) return false;

    const std::string gateId = "EAST_" + std::to_string(targetGateNumber);
    const AirportParkingStand* targetGate = nullptr;
    for (const AirportParkingStand& stand : terminal.GetParkingStands())
    {
        if (stand.id == gateId)
        {
            targetGate = &stand;
            break;
        }
    }
    if (targetGate == nullptr) return false;

    waypoints.push_back(startPosition);
    float routeHeading = startHeadingDegrees;

    // The tug is parked in the lower section's outer row. Drive forward into
    // its aisle, then leave through the same west end as the follow-me car.
    constexpr float LowerSectionAisleZ = -97.0f;
    constexpr float ParkingLotExitX = 150.0f;
    constexpr float SouthOfTerminalZ = -220.0f;
    AppendRouteLine(waypoints, routeHeading,
        LowerSectionAisleZ - startPosition.y);
    AppendRouteLine(waypoints, routeHeading, 4.0f);
    AppendRouteTurn(waypoints, routeHeading, 4.0f, -1);
    AppendRouteLine(waypoints, routeHeading,
        waypoints.back().x - ParkingLotExitX);

    // Continue south around the terminal's east side, as the follow-me route
    // does after reaching the shared parking exit.
    AppendRouteTurn(waypoints, routeHeading, 12.0f, -1);
    AppendRouteLine(waypoints, routeHeading,
        waypoints.back().y - SouthOfTerminalZ);
    AppendRouteTurn(waypoints, routeHeading, 12.0f, 1);

    // Approach from the apron side of the east wing, outside the aircraft
    // stand rectangles. The tug and its trailing carts remain west of them.
    constexpr float StandClearanceX = 20.0f;
    const float approachX = targetGate->position.x - StandClearanceX;
    AppendRouteLine(waypoints, routeHeading,
        waypoints.back().x - (approachX + 10.0f));
    AppendRouteTurn(waypoints, routeHeading, 10.0f, 1);
    const float approachZ = targetGate->position.z;
    AppendRouteLine(waypoints, routeHeading,
        approachZ - waypoints.back().y);

    gateNumber = targetGateNumber;
    return true;
}

bool ServiceVehicleRoute::Advance(Vector2& position, float& headingDegrees,
    std::size_t& nextWaypoint, float deltaTime, float speed,
    float maximumSteeringRate) const
{
    if (waypoints.empty() || deltaTime <= 0.0f || speed <= 0.0f)
        return false;

    float remainingTime = deltaTime;
    while (nextWaypoint < waypoints.size() && remainingTime > 0.0f)
    {
        const Vector2 waypoint = waypoints[nextWaypoint];
        const float dx = waypoint.x - position.x;
        const float dz = waypoint.y - position.y;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance <= 0.001f)
        {
            ++nextWaypoint;
            continue;
        }

        const float pathHeading = std::atan2(dx, dz) / DegreesToRadians;
        const bool reversing = nextWaypoint <= reverseWaypointCount;
        headingDegrees = std::remainder(
            pathHeading + (reversing ? 180.0f : 0.0f), 360.0f);

        float segmentSpeed = speed;
        if (nextWaypoint + 1 < waypoints.size())
        {
            const Vector2 next = waypoints[nextWaypoint + 1];
            const float nextHeading = std::atan2(
                next.x - waypoint.x, next.y - waypoint.y);
            const float segmentHeading = std::atan2(dx, dz);
            const float turnAngle = std::abs(std::remainder(
                nextHeading - segmentHeading, 2.0f * PI));
            if (turnAngle > 0.001f)
            {
                const Vector2 previous = waypoints[nextWaypoint - 1];
                const float segmentDx = waypoint.x - previous.x;
                const float segmentDz = waypoint.y - previous.y;
                const float segmentLength = std::sqrt(
                    segmentDx * segmentDx + segmentDz * segmentDz);
                const float turnRadius = segmentLength / turnAngle;
                segmentSpeed = std::min(segmentSpeed,
                    turnRadius * maximumSteeringRate);
            }
        }

        const float travelDistance = std::min(distance,
            segmentSpeed * remainingTime);
        position.x += dx / distance * travelDistance;
        position.y += dz / distance * travelDistance;
        remainingTime -= travelDistance / segmentSpeed;
        if (travelDistance < distance) break;
        ++nextWaypoint;
    }

    return nextWaypoint >= waypoints.size();
}

const std::vector<Vector2>& ServiceVehicleRoute::GetWaypoints() const
{
    return waypoints;
}

std::size_t ServiceVehicleRoute::GetReverseWaypointCount() const
{
    return reverseWaypointCount;
}

int ServiceVehicleRoute::GetGateNumber() const
{
    return gateNumber;
}
