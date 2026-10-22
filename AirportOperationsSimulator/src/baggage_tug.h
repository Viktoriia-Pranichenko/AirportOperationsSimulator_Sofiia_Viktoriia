#pragma once

#include "service_vehicle_route.h"

#include <cstddef>

class AirportTerminal;

class BaggageTug
{
public:
    BaggageTug(float positionX, float positionZ);
    void SetHeadingDegrees(float headingDegrees);

    bool MoveTowardsGate(int gateNumber, const AirportTerminal& terminal,
        float deltaTime);
    void Draw() const;

private:
    float positionX;
    float positionZ;
    float headingDegrees{0.0f};
    ServiceVehicleRoute route;
    std::size_t nextRoutePoint{0};
};
