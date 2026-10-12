#pragma once

class FollowMeCar
{
public:
    FollowMeCar(float positionX, float positionZ);
    void Draw() const;

private:
    float positionX;
    float positionZ;
};

class AircraftPushbackTug
{
public:
    AircraftPushbackTug(float positionX, float positionZ);
    void Draw() const;

private:
    float positionX;
    float positionZ;
};

class BaggageBeltLoader
{
public:
    BaggageBeltLoader(float positionX, float positionZ);
    void Draw() const;

private:
    float positionX;
    float positionZ;
};
