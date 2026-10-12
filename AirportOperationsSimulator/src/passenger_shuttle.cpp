#include "passenger_shuttle.h"

#include "raylib.h"

#include <initializer_list>

namespace
{
    constexpr Color BusBody{225, 226, 216, 255};
    constexpr Color BusShade{159, 171, 169, 255};
    constexpr Color Window{43, 104, 128, 255};
    constexpr Color WindowHighlight{111, 165, 177, 255};
    constexpr Color Tire{38, 43, 44, 255};
    constexpr Color Hub{157, 164, 157, 255};
    constexpr Color Accent{218, 174, 66, 255};

    void DrawWheel(float x, float z)
    {
        DrawCylinderEx(
            {x - 0.16f, 0.41f, z},
            {x + 0.16f, 0.41f, z},
            0.42f,
            0.42f,
            10,
            Tire);
        DrawCylinderEx(
            {x - 0.18f, 0.41f, z},
            {x - 0.08f, 0.41f, z},
            0.19f,
            0.19f,
            8,
            Hub);
    }
}

PassengerShuttle::PassengerShuttle(float positionX, float positionZ)
    : positionX(positionX), positionZ(positionZ)
{
}

void PassengerShuttle::Draw() const
{
    const float x = positionX;
    const float z = positionZ;

    DrawCubeV({x, 0.98f, z}, {2.62f, 0.86f, 9.2f}, BusShade);
    DrawCubeV({x, 1.56f, z}, {2.55f, 1.12f, 9.0f}, BusBody);
    DrawCubeV({x, 2.15f, z - 0.08f}, {2.48f, 0.12f, 8.75f}, BusBody);

    DrawCubeV({x, 1.85f, z + 4.54f}, {2.15f, 0.78f, 0.06f}, Window);
    DrawCubeV({x, 1.82f, z + 4.59f}, {0.13f, 0.84f, 0.07f}, BusShade);
    DrawCubeV({x, 1.25f, z + 4.67f}, {2.42f, 0.16f, 0.22f}, BusShade);
    DrawCubeV({x, 1.84f, z - 4.54f}, {1.9f, 0.64f, 0.06f}, Window);
    DrawCubeV({x, 1.25f, z - 4.67f}, {2.42f, 0.16f, 0.22f}, BusShade);

    for (float side : {-1.0f, 1.0f})
    {
        for (float windowZ : {-3.0f, -1.45f, 0.1f, 1.65f, 3.1f})
        {
            DrawCubeV(
                {x + side * 1.31f, 1.83f, z + windowZ},
                {0.055f, 0.68f, 1.2f},
                Window);
            DrawCubeV(
                {x + side * 1.345f, 2.08f, z + windowZ},
                {0.025f, 0.06f, 1.08f},
                WindowHighlight);
        }

        for (float axleZ : {-3.25f, 3.25f})
            DrawWheel(x + side * 1.33f, z + axleZ);
    }

    for (float lampX : {-0.82f, 0.82f})
    {
        DrawCubeV({x + lampX, 1.42f, z + 4.79f}, {0.34f, 0.18f, 0.08f}, Accent);
        DrawCubeV({x + lampX, 1.43f, z - 4.79f}, {0.3f, 0.13f, 0.08f}, BusBody);
    }
}
