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
    const AirportParkingStand* GetAdjacentGate(
        const std::string& gateId, int direction) const;
    const AirportParkingStand* GetGateAtScreenPosition(
        Vector2 screenPosition, const Camera3D& camera) const;
    bool GetGateMarkingPosition(const std::string& gateId, Vector3& position) const;
    void DrawGateHighlight(const std::string& gateId) const;
};
