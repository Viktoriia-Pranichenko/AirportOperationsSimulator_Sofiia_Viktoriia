#pragma once

class AirportTerminal;

class ServiceVehiclePosition
{
public:
    ServiceVehiclePosition(float positionX, float positionZ);
    void SetHeadingDegrees(float headingDegrees);
    bool MoveTowardsGate(int gateNumber, const AirportTerminal& terminal,
        float deltaTime);

protected:
    float positionX;
    float positionZ;
    float headingDegrees{0.0f};
};

class AircraftPushbackTug : public ServiceVehiclePosition
{
public:
    AircraftPushbackTug(float positionX, float positionZ);
    void Draw() const;

};

class BaggageBeltLoader : public ServiceVehiclePosition
{
public:
    BaggageBeltLoader(float positionX, float positionZ);
    void Draw() const;

};

class CateringTruck : public ServiceVehiclePosition
{
public:
    CateringTruck(float positionX, float positionZ);
    void Draw() const;

};

class WaterServiceTruck : public ServiceVehiclePosition
{
public:
    WaterServiceTruck(float positionX, float positionZ);
    void Draw() const;

};

class MaintenancePickupTruck : public ServiceVehiclePosition
{
public:
    MaintenancePickupTruck(float positionX, float positionZ);
    void Draw() const;

};

class PassengerBoardingStairs : public ServiceVehiclePosition
{
public:
    PassengerBoardingStairs(float positionX, float positionZ);
    void Draw() const;

};

class AircraftRefuellingTanker : public ServiceVehiclePosition
{
public:
    AircraftRefuellingTanker(float positionX, float positionZ);
    void Draw() const;

};
