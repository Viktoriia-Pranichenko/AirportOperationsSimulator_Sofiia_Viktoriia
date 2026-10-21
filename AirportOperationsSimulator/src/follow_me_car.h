#pragma once

#include "airport_service_vehicles.h"
#include "service_vehicle_route.h"

#include <cstddef>

class AirportTerminal;

class FollowMeCar : public ServiceVehiclePosition
{
public:
    FollowMeCar(float positionX, float positionZ);
    bool MoveTowardsGate(int gateNumber, const AirportTerminal& terminal,
        float deltaTime);
    void Draw() const;

    // Other service vehicles can follow these same sampled road waypoints.
    const ServiceVehicleRoute& GetRoute() const;

private:
    ServiceVehicleRoute route;
    std::size_t nextRoutePoint{0};
    float headingDegrees{0.0f};
};
