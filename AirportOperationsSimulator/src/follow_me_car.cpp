#include "follow_me_car.h"

#include "airport_terminal.h"
#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

namespace
{
    constexpr Color FollowYellow{226, 181, 48, 255};
    constexpr Color FollowBlack{39, 43, 43, 255};
    constexpr Color Glass{42, 100, 125, 255};
    constexpr Color Trim{195, 201, 192, 255};
    constexpr Color Tire{37, 42, 43, 255};
    constexpr Color WheelHub{162, 166, 155, 255};
    constexpr float DegreesToRadians = PI / 180.0f;
    constexpr float TaxiMovementSpeed = 13.0f;
    constexpr float TaxiAcceleration = 8.0f;
    constexpr float TaxiDeceleration = 10.0f;
    constexpr float TaxiMaximumTurnRate = 95.0f;
    constexpr float BeaconFlashPeriod = 0.8f;

    float BeaconIntensity(float phase)
    {
        constexpr float fadeDuration = 0.06f;
        constexpr float onDuration = BeaconFlashPeriod * 0.5f;
        const auto smoothStep = [](float value)
        {
            const float clamped = std::clamp(value, 0.0f, 1.0f);
            return clamped * clamped * (3.0f - 2.0f * clamped);
        };
        if (phase < fadeDuration)
            return smoothStep(phase / fadeDuration);
        if (phase < onDuration - fadeDuration) return 1.0f;
        if (phase < onDuration)
            return 1.0f - smoothStep(
                (phase - (onDuration - fadeDuration)) / fadeDuration);
        return 0.0f;
    }

    void DrawWheel(float x, float y, float z, float radius, float side,
        float spinDegrees, float steeringDegrees)
    {
        rlPushMatrix();
        rlTranslatef(x, y, z);
        rlRotatef(steeringDegrees, 0.0f, 1.0f, 0.0f);
        rlRotatef(spinDegrees, 1.0f, 0.0f, 0.0f);

        DrawCylinderEx({-0.14f, 0.0f, 0.0f}, {0.14f, 0.0f, 0.0f},
            radius, radius, 10, Tire);
        DrawCylinderEx({side * 0.07f, 0.0f, 0.0f},
            {side * 0.16f, 0.0f, 0.0f},
            radius * 0.46f, radius * 0.46f, 8, WheelHub);
        for (int spoke = 0; spoke < 5; ++spoke)
        {
            const float spokeAngle = 2.0f * PI * spoke / 5.0f;
            const float innerY = std::cos(spokeAngle) * radius * 0.16f;
            const float innerZ = std::sin(spokeAngle) * radius * 0.16f;
            const float outerY = std::cos(spokeAngle) * radius * 0.84f;
            const float outerZ = std::sin(spokeAngle) * radius * 0.84f;
            DrawCylinderEx({side * 0.145f, innerY, innerZ},
                {side * 0.145f, outerY, outerZ},
                0.04f, 0.04f, 6,
                spoke == 0 ? Color{255, 222, 150, 255} : Trim);
        }
        rlPopMatrix();
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
        SetWaypoints(route.GetWaypoints(), route.GetReverseWaypointCount());
    }

    return AdvanceRoute(deltaTime);
}

void FollowMeCar::SetWaypoints(std::vector<Vector2> waypoints,
    std::size_t reverseWaypointCount)
{
    activeWaypoints = std::move(waypoints);
    this->reverseWaypointCount = std::min(reverseWaypointCount,
        activeWaypoints.empty() ? 0 : activeWaypoints.size() - 1);
    nextRoutePoint = activeWaypoints.empty() ? 0 : 1;
}

bool FollowMeCar::AdvanceRoute(float deltaTime)
{
    if (deltaTime <= 0.0f || activeWaypoints.empty()) return false;
    UpdateBeaconFlash(deltaTime);
    constexpr float MovementSpeed = 12.0f;
    constexpr float MaximumSteeringRate = 95.0f;
    float remainingTime = deltaTime;
    while (nextRoutePoint < activeWaypoints.size() && remainingTime > 0.0f)
    {
        const Vector2 waypoint = activeWaypoints[nextRoutePoint];
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
            nextRoutePoint <= reverseWaypointCount;
        const float desiredHeading = std::remainder(
            pathHeading + (reversing ? 180.0f : 0.0f), 360.0f);
        const float headingDelta = std::remainder(
            desiredHeading - headingDegrees, 360.0f);
        float segmentSpeed = MovementSpeed;
        if (nextRoutePoint + 1 < activeWaypoints.size())
        {
            const Vector2 nextWaypoint = activeWaypoints[nextRoutePoint + 1];
            const float nextHeading = std::atan2(
                nextWaypoint.x - waypoint.x, nextWaypoint.y - waypoint.y);
            const float segmentHeading = std::atan2(dx, dz);
            const float turnAngle = std::abs(std::remainder(
                nextHeading - segmentHeading, 2.0f * PI));
            if (turnAngle > 0.001f)
            {
                const Vector2 previousWaypoint = activeWaypoints[nextRoutePoint - 1];
                const float segmentDx = waypoint.x - previousWaypoint.x;
                const float segmentDz = waypoint.y - previousWaypoint.y;
                const float segmentLength = std::sqrt(
                    segmentDx * segmentDx + segmentDz * segmentDz);
                const float turnRadius = segmentLength / turnAngle;
                segmentSpeed = std::min(segmentSpeed,
                    turnRadius * MaximumSteeringRate);
            }
        }

        const float turnTime = std::min(remainingTime,
            distance / segmentSpeed);
        headingDegrees = std::remainder(headingDegrees +
            std::clamp(headingDelta, -MaximumSteeringRate * turnTime,
                MaximumSteeringRate * turnTime), 360.0f);
        const float desiredSteering = std::clamp(
            headingDelta * 0.4f, -28.0f, 28.0f);
        steeringAngleDegrees += std::clamp(
            desiredSteering - steeringAngleDegrees,
            -100.0f * turnTime, 100.0f * turnTime);

        const float travelDistance = std::min(distance,
            segmentSpeed * remainingTime);
        wheelRotationDegrees = std::remainder(
            wheelRotationDegrees + travelDistance / 0.38f /
                DegreesToRadians * (reversing ? -1.0f : 1.0f),
            360.0f);
        positionX += dx / distance * travelDistance;
        positionZ += dz / distance * travelDistance;
        remainingTime -= travelDistance / segmentSpeed;
        if (travelDistance < distance) break;
        ++nextRoutePoint;
    }

    return nextRoutePoint >= activeWaypoints.size();
}

bool FollowMeCar::SetTaxiRoute(const std::vector<Vector2>& waypoints,
    bool startAtRouteBeginning)
{
    if (waypoints.size() < 2) return false;
    bool sameRoute = taxiRouteActive &&
        taxiWaypoints.size() == waypoints.size();
    if (sameRoute)
    {
        for (std::size_t index = 0; index < waypoints.size(); ++index)
        {
            if (taxiWaypoints[index].x != waypoints[index].x ||
                taxiWaypoints[index].y != waypoints[index].y)
            {
                sameRoute = false;
                break;
            }
        }
    }
    if (sameRoute) return true;

    taxiWaypoints = waypoints;
    taxiWaypointDistances.assign(taxiWaypoints.size(), 0.0f);
    for (std::size_t index = 1; index < taxiWaypoints.size(); ++index)
    {
        const float dx = taxiWaypoints[index].x - taxiWaypoints[index - 1].x;
        const float dz = taxiWaypoints[index].y - taxiWaypoints[index - 1].y;
        taxiWaypointDistances[index] = taxiWaypointDistances[index - 1] +
            std::sqrt(dx * dx + dz * dz);
    }
    if (taxiWaypointDistances.back() <= 0.001f)
    {
        ClearTaxiRoute();
        return false;
    }

    taxiRouteActive = true;
    taxiSpeed = 0.0f;
    if (startAtRouteBeginning)
    {
        taxiRouteProgress = 0.0f;
        const Vector2 start = GetTaxiPositionAtProgress(taxiRouteProgress);
        SetPosition(start.x, start.y);
        const Vector2 direction = GetTaxiDirectionAtProgress(taxiRouteProgress);
        headingDegrees = std::atan2(direction.x, direction.y) /
            DegreesToRadians;
        return true;
    }

    float nearestDistanceSquared = std::numeric_limits<float>::max();
    float nearestProgress = 0.0f;
    Vector2 nearestPosition = taxiWaypoints.front();
    const Vector2 currentPosition = GetPosition();
    for (std::size_t index = 1; index < taxiWaypoints.size(); ++index)
    {
        const Vector2 start = taxiWaypoints[index - 1];
        const Vector2 end = taxiWaypoints[index];
        const float dx = end.x - start.x;
        const float dz = end.y - start.y;
        const float segmentLengthSquared = dx * dx + dz * dz;
        if (segmentLengthSquared <= 0.001f) continue;
        const float projection = std::clamp(
            ((currentPosition.x - start.x) * dx +
                (currentPosition.y - start.y) * dz) /
                segmentLengthSquared, 0.0f, 1.0f);
        const Vector2 projected{start.x + dx * projection,
            start.y + dz * projection};
        const float offsetX = currentPosition.x - projected.x;
        const float offsetZ = currentPosition.y - projected.y;
        const float distanceSquared = offsetX * offsetX + offsetZ * offsetZ;
        if (distanceSquared < nearestDistanceSquared)
        {
            nearestDistanceSquared = distanceSquared;
            nearestPosition = projected;
            nearestProgress = taxiWaypointDistances[index - 1] +
                std::sqrt(segmentLengthSquared) * projection;
        }
    }
    taxiRouteProgress = nearestProgress;
    SetPosition(nearestPosition.x, nearestPosition.y);
    return true;
}

void FollowMeCar::ClearTaxiRoute()
{
    taxiWaypoints.clear();
    taxiWaypointDistances.clear();
    taxiRouteProgress = 0.0f;
    taxiSpeed = 0.0f;
    taxiRouteActive = false;
}

bool FollowMeCar::HasTaxiRoute() const
{
    return taxiRouteActive;
}

float FollowMeCar::GetTaxiRouteProgress() const
{
    return taxiRouteProgress;
}

float FollowMeCar::GetTaxiRouteLength() const
{
    return taxiWaypointDistances.empty() ? 0.0f :
        taxiWaypointDistances.back();
}

float FollowMeCar::GetTaxiLeadDistance() const
{
    return taxiLeadDistance;
}

void FollowMeCar::SetTaxiLeadDistance(float distance)
{
    taxiLeadDistance = std::max(0.0f, distance);
}

float FollowMeCar::GetTaxiTargetProgress(float aircraftProgress) const
{
    return std::clamp(aircraftProgress + taxiLeadDistance,
        0.0f, GetTaxiRouteLength());
}

float FollowMeCar::GetNextTaxiRouteProgress(float targetProgress,
    float deltaTime) const
{
    if (!taxiRouteActive || deltaTime <= 0.0f) return taxiRouteProgress;
    const float target = std::clamp(targetProgress,
        taxiRouteProgress, GetTaxiRouteLength());
    if (target <= taxiRouteProgress) return taxiRouteProgress;

    const float nextSpeed = CalculateNextTaxiSpeed(target, deltaTime);
    const float travelDistance =
        (taxiSpeed + nextSpeed) * 0.5f * deltaTime;
    float nextProgress = std::min(target,
        taxiRouteProgress + travelDistance);
    const auto nextWaypoint = std::upper_bound(taxiWaypointDistances.begin(),
        taxiWaypointDistances.end(), taxiRouteProgress + 0.001f);
    if (nextWaypoint != taxiWaypointDistances.end())
        nextProgress = std::min(nextProgress, *nextWaypoint);
    return nextProgress;
}

Vector2 FollowMeCar::GetTaxiPositionAtProgress(float progress) const
{
    if (taxiWaypoints.empty()) return GetPosition();
    if (taxiWaypoints.size() == 1) return taxiWaypoints.front();
    const float clampedProgress = std::clamp(progress,
        0.0f, GetTaxiRouteLength());
    const auto nextWaypoint = std::upper_bound(taxiWaypointDistances.begin(),
        taxiWaypointDistances.end(), clampedProgress);
    if (nextWaypoint == taxiWaypointDistances.end())
        return taxiWaypoints.back();
    const std::size_t endIndex = static_cast<std::size_t>(
        std::distance(taxiWaypointDistances.begin(), nextWaypoint));
    if (endIndex == 0) return taxiWaypoints.front();
    const std::size_t startIndex = endIndex - 1;
    const float segmentLength = taxiWaypointDistances[endIndex] -
        taxiWaypointDistances[startIndex];
    if (segmentLength <= 0.001f) return taxiWaypoints[endIndex];
    const float ratio = (clampedProgress -
        taxiWaypointDistances[startIndex]) / segmentLength;
    const Vector2 start = taxiWaypoints[startIndex];
    const Vector2 end = taxiWaypoints[endIndex];
    return {start.x + (end.x - start.x) * ratio,
        start.y + (end.y - start.y) * ratio};
}

void FollowMeCar::AdvanceTaxiRoute(float targetProgress, float deltaTime,
    bool allowMovement)
{
    if (!taxiRouteActive || deltaTime <= 0.0f) return;
    UpdateBeaconFlash(deltaTime);
    const Vector2 direction = GetTaxiDirectionAtProgress(taxiRouteProgress);
    const float desiredHeading = std::atan2(direction.x, direction.y) /
        DegreesToRadians;
    const float headingDelta = std::remainder(
        desiredHeading - headingDegrees, 360.0f);

    if (allowMovement)
    {
        const float target = std::clamp(targetProgress,
            taxiRouteProgress, GetTaxiRouteLength());
        const float nextSpeed = CalculateNextTaxiSpeed(target, deltaTime);
        const float previousProgress = taxiRouteProgress;
        taxiRouteProgress = GetNextTaxiRouteProgress(target, deltaTime);
        taxiSpeed = taxiRouteProgress >= target - 0.001f ? 0.0f : nextSpeed;
        const float distanceTravelled = taxiRouteProgress - previousProgress;
        wheelRotationDegrees = std::remainder(
            wheelRotationDegrees + distanceTravelled / 0.38f /
                DegreesToRadians, 360.0f);
        const Vector2 position = GetTaxiPositionAtProgress(taxiRouteProgress);
        SetPosition(position.x, position.y);
    }
    else
    {
        taxiSpeed = std::max(0.0f, taxiSpeed -
            TaxiDeceleration * deltaTime);
    }

    const float desiredSteering = std::clamp(
        headingDelta * 0.4f, -28.0f, 28.0f);
    steeringAngleDegrees += std::clamp(
        desiredSteering - steeringAngleDegrees,
        -100.0f * deltaTime, 100.0f * deltaTime);
    headingDegrees = std::remainder(headingDegrees + std::clamp(headingDelta,
        -TaxiMaximumTurnRate * deltaTime,
        TaxiMaximumTurnRate * deltaTime), 360.0f);
}

float FollowMeCar::CalculateNextTaxiSpeed(float targetProgress,
    float deltaTime) const
{
    const float remainingDistance = std::max(0.0f,
        targetProgress - taxiRouteProgress);
    const float desiredSpeed = std::min(TaxiMovementSpeed,
        std::sqrt(2.0f * TaxiDeceleration * remainingDistance));
    float cornerSpeedLimit = TaxiMovementSpeed;
    const auto nextWaypoint = std::upper_bound(taxiWaypointDistances.begin(),
        taxiWaypointDistances.end(), taxiRouteProgress + 0.001f);
    if (nextWaypoint != taxiWaypointDistances.end())
    {
        const std::size_t cornerIndex = static_cast<std::size_t>(
            std::distance(taxiWaypointDistances.begin(), nextWaypoint));
        if (cornerIndex + 1 < taxiWaypoints.size())
        {
            const Vector2 corner = taxiWaypoints[cornerIndex];
            const Vector2 previous = taxiWaypoints[cornerIndex - 1];
            const Vector2 next = taxiWaypoints[cornerIndex + 1];
            const float incomingX = corner.x - previous.x;
            const float incomingZ = corner.y - previous.y;
            const float outgoingX = next.x - corner.x;
            const float outgoingZ = next.y - corner.y;
            const float angle = std::abs(std::atan2(
                incomingX * outgoingZ - incomingZ * outgoingX,
                incomingX * outgoingX + incomingZ * outgoingZ));
            const float distanceToCorner =
                taxiWaypointDistances[cornerIndex] - taxiRouteProgress;
            constexpr float TurnSlowdownDistance = 12.0f;
            if (angle > 0.15f && distanceToCorner < TurnSlowdownDistance)
            {
                const float severity = std::clamp(angle / PI, 0.0f, 1.0f);
                const float turnSpeed = std::max(6.0f,
                    TaxiMovementSpeed * (1.0f - 0.45f * severity));
                const float turnProgress = std::clamp(
                    distanceToCorner / TurnSlowdownDistance, 0.0f, 1.0f);
                cornerSpeedLimit = turnSpeed +
                    (TaxiMovementSpeed - turnSpeed) * turnProgress;
            }
        }
    }
    const float speedLimit = std::min(desiredSpeed, cornerSpeedLimit);
    const float speedChange = std::clamp(speedLimit - taxiSpeed,
        -TaxiDeceleration * deltaTime,
        TaxiAcceleration * deltaTime);
    return std::max(0.0f, taxiSpeed + speedChange);
}

Vector2 FollowMeCar::GetTaxiDirectionAtProgress(float progress) const
{
    if (taxiWaypoints.size() < 2) return {0.0f, 1.0f};
    const auto nextWaypoint = std::upper_bound(taxiWaypointDistances.begin(),
        taxiWaypointDistances.end(), progress);
    std::size_t endIndex = nextWaypoint == taxiWaypointDistances.end() ?
        taxiWaypoints.size() - 1 :
        static_cast<std::size_t>(std::distance(
            taxiWaypointDistances.begin(), nextWaypoint));
    while (endIndex > 0)
    {
        const Vector2 start = taxiWaypoints[endIndex - 1];
        const Vector2 end = taxiWaypoints[endIndex];
        const float dx = end.x - start.x;
        const float dz = end.y - start.y;
        const float length = std::sqrt(dx * dx + dz * dz);
        if (length > 0.001f) return {dx / length, dz / length};
        --endIndex;
    }
    return {0.0f, 1.0f};
}

void FollowMeCar::UpdateBeaconFlash(float deltaTime)
{
    beaconFlashPhase = std::fmod(beaconFlashPhase + deltaTime,
        BeaconFlashPeriod);
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
            DrawWheel(side * 1.04f, 0.38f, axleZ, 0.38f, side,
                wheelRotationDegrees, axleZ > 0.0f ?
                    steeringAngleDegrees : 0.0f);
    }

    DrawCubeV({0.0f, 2.12f, -0.35f}, {0.32f, 0.08f, 0.3f}, FollowYellow);
    const float beaconIntensity = BeaconIntensity(beaconFlashPhase);
    const Color beaconColor{
        static_cast<unsigned char>(std::lround(55.0f +
            200.0f * beaconIntensity)),
        static_cast<unsigned char>(std::lround(4.0f +
            48.0f * beaconIntensity)),
        static_cast<unsigned char>(std::lround(7.0f +
            30.0f * beaconIntensity)),
        255
    };
    DrawCubeV({0.0f, 2.27f, -0.35f}, {0.24f, 0.2f, 0.22f},
        beaconColor);
    if (beaconIntensity > 0.01f)
    {
        DrawSphereEx({0.0f, 2.27f, -0.35f}, 0.34f, 8, 10,
            Color{255, 20, 12, static_cast<unsigned char>(
                std::lround(45.0f * beaconIntensity))});
    }
    DrawCubeV({-0.68f, 1.14f, 2.34f}, {0.32f, 0.16f, 0.06f}, Trim);
    DrawCubeV({0.68f, 1.14f, 2.34f}, {0.32f, 0.16f, 0.06f}, Trim);
    rlPopMatrix();
}
