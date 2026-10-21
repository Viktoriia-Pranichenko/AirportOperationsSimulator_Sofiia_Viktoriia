#pragma once

#include "raylib.h"

#include <cstddef>
#include <vector>

class AirportTerminal;

// A reusable, sampled path that service vehicles can follow in order.
class ServiceVehicleRoute
{
public:
    bool BuildToEastWingGate(int gateNumber, const AirportTerminal& terminal,
        Vector2 startPosition, float startHeadingDegrees);
    bool BuildToEastWingGateFromBaggageParking(int gateNumber,
        const AirportTerminal& terminal, Vector2 startPosition,
        float startHeadingDegrees);
    bool Advance(Vector2& position, float& headingDegrees,
        std::size_t& nextWaypoint, float deltaTime, float speed,
        float maximumSteeringRate) const;

    const std::vector<Vector2>& GetWaypoints() const;
    std::size_t GetReverseWaypointCount() const;
    int GetGateNumber() const;

private:
    std::vector<Vector2> waypoints;
    std::size_t reverseWaypointCount{0};
    int gateNumber{-1};
};
