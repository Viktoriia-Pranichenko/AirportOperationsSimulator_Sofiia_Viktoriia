#pragma once

class AirportFireTruck
{
public:
    AirportFireTruck(float positionX, float positionZ);

    void Draw() const;

private:
    float positionX;
    float positionZ;
};
