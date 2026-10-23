#include "car_parking_lot.h"

#include "raylib.h"

namespace
{
    constexpr float ParkingLeft = 162.0f;
    constexpr float ParkingSpaceWidth = 5.1f;
    constexpr int ParkingSpaceCount = 14;
    constexpr float ParkingSpaceDepth = 10.5f;
    constexpr float AisleCenterZ = -55.0f;
    constexpr float AisleWidth = 20.0f;
    constexpr float PaintY = 0.06f;
    constexpr float PaintThickness = 0.04f;
    constexpr float PaintWidth = 0.5f;
    constexpr Color ParkingMarking{231, 232, 219, 255};

    void DrawParkingRow(float aisleEdgeZ, float outwardDirection, float offsetZ)
    {
        aisleEdgeZ += offsetZ;
        const float rowCenterZ = aisleEdgeZ + outwardDirection * ParkingSpaceDepth * 0.5f;
        for (int space = 0; space <= ParkingSpaceCount; ++space)
        {
            const float x = ParkingLeft + space * ParkingSpaceWidth;
            DrawCubeV(
                {x, PaintY, rowCenterZ},
                {PaintWidth, PaintThickness, ParkingSpaceDepth},
                ParkingMarking);
        }

        const float backZ = aisleEdgeZ + outwardDirection * ParkingSpaceDepth;
        DrawCubeV(
            {ParkingLeft + ParkingSpaceCount * ParkingSpaceWidth * 0.5f,
                PaintY, backZ},
            {ParkingSpaceCount * ParkingSpaceWidth, PaintThickness, PaintWidth},
            ParkingMarking);
    }
}

void CarParkingLot::Draw() const
{
    const float aisleHalfWidth = AisleWidth * 0.5f;
    constexpr float RowBlockOffsetZ = -42.0f;
    constexpr float rowOffsets[] = {0.0f, RowBlockOffsetZ};
    for (float offsetZ : rowOffsets)
    {
        DrawParkingRow(AisleCenterZ - aisleHalfWidth, -1.0f, offsetZ);
        DrawParkingRow(AisleCenterZ + aisleHalfWidth, 1.0f, offsetZ);
    }
}
