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
    bool SetTaxiRoute(const std::vector<Vector2>& waypoints,
        bool startAtRouteBeginning);
    void ClearTaxiRoute();
    bool HasTaxiRoute() const;
    float GetTaxiRouteProgress() const;
    float GetTaxiRouteLength() const;
    float GetTaxiLeadDistance() const;
    void SetTaxiLeadDistance(float distance);
    float GetTaxiTargetProgress(float aircraftProgress) const;
    float GetNextTaxiRouteProgress(float targetProgress, float deltaTime) const;
    Vector2 GetTaxiPositionAtProgress(float progress) const;
    void AdvanceTaxiRoute(float targetProgress, float deltaTime,
        bool allowMovement);
    void Draw() const;

    // Other service vehicles can follow these same sampled road waypoints.
    const ServiceVehicleRoute& GetRoute() const;

private:
    ServiceVehicleRoute route;
    std::vector<Vector2> activeWaypoints;
    std::size_t nextRoutePoint{0};
    std::size_t reverseWaypointCount{0};
    std::vector<Vector2> taxiWaypoints;
    std::vector<float> taxiWaypointDistances;
    float taxiRouteProgress{0.0f};
    float taxiSpeed{0.0f};
    float taxiLeadDistance{28.0f};
    float wheelRotationDegrees{0.0f};
    float steeringAngleDegrees{0.0f};
    bool taxiRouteActive{false};

    float CalculateNextTaxiSpeed(float targetProgress,
        float deltaTime) const;
    Vector2 GetTaxiDirectionAtProgress(float progress) const;
};
