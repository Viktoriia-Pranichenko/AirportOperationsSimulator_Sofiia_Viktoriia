#pragma once

class AirportFireTruck
{
public:
    AirportFireTruck(float positionX, float positionZ, float yawDegrees = 0.0f);

    void Draw() const;

private:
    float positionX;
    float positionZ;
    float yawDegrees;
};
