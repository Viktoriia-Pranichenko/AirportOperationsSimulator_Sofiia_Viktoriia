#pragma once

class PassengerShuttle
{
public:
    PassengerShuttle(float positionX, float positionZ);

    void Draw() const;

private:
    float positionX;
    float positionZ;
};
