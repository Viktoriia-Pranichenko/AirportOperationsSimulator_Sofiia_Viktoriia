#include "airport_fire_truck.h"

#include "raylib.h"

#include <initializer_list>

namespace
{
    constexpr Color FireRed{178, 46, 39, 255};
    constexpr Color FireRedShade{119, 38, 35, 255};
    constexpr Color Window{42, 100, 125, 255};
    constexpr Color Chrome{183, 190, 183, 255};
    constexpr Color Tire{36, 40, 41, 255};
    constexpr Color Hub{159, 164, 155, 255};
    constexpr Color WarningLight{226, 176, 52, 255};
    constexpr Color Ladder{200, 204, 193, 255};

    void DrawWheel(float x, float z)
    {
        DrawCylinderEx(
            {x - 0.18f, 0.43f, z},
            {x + 0.18f, 0.43f, z},
            0.44f,
            0.44f,
            10,
            Tire);
        DrawCylinderEx(
            {x - 0.20f, 0.43f, z},
            {x - 0.09f, 0.43f, z},
            0.2f,
            0.2f,
            8,
            Hub);
    }
}

AirportFireTruck::AirportFireTruck(float positionX, float positionZ)
    : positionX(positionX), positionZ(positionZ)
{
}

void AirportFireTruck::Draw() const
{
    const float x = positionX;
    const float z = positionZ;

    DrawCubeV({x, 0.98f, z}, {2.72f, 0.92f, 9.35f}, FireRedShade);
    DrawCubeV({x, 1.32f, z - 1.1f}, {2.7f, 1.1f, 5.6f}, FireRed);
    DrawCubeV({x, 1.56f, z + 2.85f}, {2.68f, 1.9f, 3.45f}, FireRed);
    DrawCubeV({x, 2.56f, z + 2.6f}, {2.74f, 0.12f, 3.9f}, FireRedShade);

    DrawCubeV({x, 1.93f, z + 4.61f}, {2.2f, 0.88f, 0.06f}, Window);
    DrawCubeV({x, 1.93f, z + 4.65f}, {0.12f, 0.94f, 0.07f}, Chrome);
    DrawCubeV({x, 1.33f, z + 4.72f}, {2.55f, 0.17f, 0.25f}, Chrome);

    for (float side : {-1.0f, 1.0f})
    {
        DrawCubeV(
            {x + side * 1.37f, 1.93f, z + 2.82f},
            {0.06f, 0.78f, 1.1f},
            Window);

        for (float compartmentZ : {-3.2f, -1.55f, 0.1f})
        {
            DrawCubeV(
                {x + side * 1.38f, 1.34f, z + compartmentZ},
                {0.06f, 0.72f, 1.42f},
                FireRed);
            DrawCubeV(
                {x + side * 1.415f, 1.34f, z + compartmentZ},
                {0.025f, 0.045f, 1.25f},
                Chrome);
            DrawCubeV(
                {x + side * 1.43f, 1.58f, z + compartmentZ},
                {0.035f, 0.07f, 0.16f},
                Chrome);
        }

        for (float axleZ : {-3.35f, 0.0f, 3.35f})
            DrawWheel(x + side * 1.4f, z + axleZ);
    }

    DrawCubeV({x, 2.65f, z + 2.45f}, {1.7f, 0.18f, 0.36f}, Chrome);
    DrawCubeV({x - 0.78f, 2.83f, z + 2.45f}, {0.25f, 0.18f, 0.38f}, WarningLight);
    DrawCubeV({x + 0.78f, 2.83f, z + 2.45f}, {0.25f, 0.18f, 0.38f}, WarningLight);

    for (float side : {-1.0f, 1.0f})
    {
        DrawCubeV({x + side * 0.72f, 2.82f, z - 1.1f}, {0.09f, 0.1f, 6.0f}, Ladder);
        DrawCubeV({x + side * 0.72f, 2.82f, z - 1.1f}, {1.52f, 0.1f, 0.09f}, Ladder);
        for (int rung = 0; rung < 6; ++rung)
        {
            DrawCubeV(
                {x + side * 0.72f, 2.75f, z - 3.45f + rung * 0.94f},
                {0.09f, 0.12f, 0.12f},
                Chrome);
        }
    }
}
