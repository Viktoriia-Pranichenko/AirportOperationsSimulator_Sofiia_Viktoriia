#pragma once

#include "raylib.h"

#include <string>
#include <vector>

struct AirportParkingStand
{
    std::string id;
    Vector3 position;
    float headingDegrees;
};

class AirportTerminal
{
public:
    void Draw() const;
    const std::vector<AirportParkingStand>& GetParkingStands() const;
};
