#pragma once

#include "airport_service_vehicles.h"
#include "service_vehicle_route.h"

#include <cstddef>
#include <vector>

class AirportTerminal;

class FollowMeCar : public ServiceVehiclePosition
{
public:
    FollowMeCar(float positionX, float positionZ);
    bool MoveTowardsGate(int gateNumber, const AirportTerminal& terminal,
        float deltaTime);
    void SetWaypoints(std::vector<Vector2> waypoints,
        std::size_t reverseWaypointCount = 0);
    bool AdvanceRoute(float deltaTime);
    void MoveTowardsPosition(Vector2 target, float deltaTime);
    void Draw() const;

    // Other service vehicles can follow these same sampled road waypoints.
    const ServiceVehicleRoute& GetRoute() const;

private:
    ServiceVehicleRoute route;
    std::vector<Vector2> activeWaypoints;
    std::size_t nextRoutePoint{0};
    std::size_t reverseWaypointCount{0};
    float headingDegrees{0.0f};
};
