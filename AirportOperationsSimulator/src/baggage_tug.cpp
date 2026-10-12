#include "baggage_tug.h"

#include "raylib.h"

#include <initializer_list>

namespace
{
    constexpr float TugWheelY = 0.42f;
    constexpr float CartWheelY = 0.31f;

    constexpr Color TugBody{218, 183, 75, 255};
    constexpr Color TugBodyShade{157, 126, 50, 255};
    constexpr Color CabinGlass{57, 119, 143, 255};
    constexpr Color Tire{38, 43, 44, 255};
    constexpr Color Hub{143, 151, 146, 255};
    constexpr Color CartFrame{88, 99, 99, 255};
    constexpr Color CartBed{119, 127, 119, 255};

    void DrawWheel(float centerX, float y, float centerZ, float radius)
    {
        DrawCylinderEx(
            {centerX - 0.16f, y, centerZ},
            {centerX + 0.16f, y, centerZ},
            radius,
            radius,
            10,
            Tire);
        DrawCylinderEx(
            {centerX - 0.19f, y, centerZ},
            {centerX - 0.09f, y, centerZ},
            radius * 0.48f,
            radius * 0.48f,
            8,
            Hub);
        DrawCylinderEx(
            {centerX + 0.09f, y, centerZ},
            {centerX + 0.19f, y, centerZ},
            radius * 0.48f,
            radius * 0.48f,
            8,
            Hub);
    }

    void DrawTug(float x, float z)
    {
        DrawCubeV({x, 0.72f, z}, {1.9f, 0.45f, 3.8f}, TugBodyShade);
        DrawCubeV({x, 0.99f, z + 0.72f}, {1.95f, 0.72f, 1.85f}, TugBody);
        DrawCubeV({x, 1.57f, z - 0.52f}, {1.82f, 1.25f, 1.72f}, TugBody);
        DrawCubeV({x, 2.22f, z - 0.52f}, {1.98f, 0.18f, 1.9f}, TugBodyShade);

        DrawCubeV(
            {x, 1.62f, z + 0.365f},
            {1.42f, 0.78f, 0.055f},
            CabinGlass);
        DrawCubeV(
            {x - 0.925f, 1.63f, z - 0.48f},
            {0.055f, 0.72f, 0.92f},
            CabinGlass);
        DrawCubeV(
            {x + 0.925f, 1.63f, z - 0.48f},
            {0.055f, 0.72f, 0.92f},
            CabinGlass);
        DrawCubeV(
            {x, 0.52f, z + 1.82f},
            {2.1f, 0.24f, 0.22f},
            CartFrame);

        for (float side : {-1.0f, 1.0f})
        {
            for (float axleZ : {-1.15f, 1.15f})
            {
                DrawWheel(x + side * 1.0f, TugWheelY, z + axleZ, 0.43f);
            }
        }

        DrawCylinderEx(
            {x, 0.52f, z - 1.82f},
            {x, 0.52f, z - 2.75f},
            0.12f,
            0.12f,
            8,
            CartFrame);
    }

    void DrawLuggageCart(float x, float centerZ, Color luggageColor, Color secondLuggageColor)
    {
        DrawCubeV({x, 0.93f, centerZ}, {1.65f, 0.16f, 3.3f}, CartBed);

        for (float side : {-1.0f, 1.0f})
        {
            DrawCubeV(
                {x + side * 0.77f, 1.34f, centerZ},
                {0.12f, 0.72f, 3.15f},
                CartFrame);
            DrawCubeV(
                {x + side * 0.77f, 1.74f, centerZ},
                {0.12f, 0.08f, 3.15f},
                CartFrame);
            DrawWheel(x + side * 0.88f, CartWheelY, centerZ - 0.98f, 0.32f);
            DrawWheel(x + side * 0.88f, CartWheelY, centerZ + 0.98f, 0.32f);
        }

        for (float axleZ : {-0.98f, 0.98f})
        {
            DrawCylinderEx(
                {x - 0.88f, CartWheelY, centerZ + axleZ},
                {x + 0.88f, CartWheelY, centerZ + axleZ},
                0.07f,
                0.07f,
                8,
                CartFrame);
            for (float side : {-1.0f, 1.0f})
            {
                DrawCubeV(
                    {x + side * 0.68f, 0.62f, centerZ + axleZ},
                    {0.12f, 0.48f, 0.14f},
                    CartFrame);
            }
        }

        DrawCubeV(
            {x - 0.36f, 1.55f, centerZ - 0.52f},
            {0.62f, 1.08f, 1.15f},
            luggageColor);
        DrawCubeV(
            {x + 0.34f, 1.43f, centerZ + 0.58f},
            {0.68f, 0.84f, 1.08f},
            secondLuggageColor);

        DrawCylinderEx(
            {x, 0.52f, centerZ + 1.66f},
            {x, 0.52f, centerZ + 2.58f},
            0.095f,
            0.095f,
            8,
            CartFrame);
    }

    void ConnectCarts(float x, float rearCartZ, float frontCartZ)
    {
        DrawCylinderEx(
            {x, 0.52f, rearCartZ - 1.66f},
            {x, 0.52f, frontCartZ + 2.58f},
            0.095f,
            0.095f,
            8,
            CartFrame);
    }
}

BaggageTug::BaggageTug(float x, float z)
    : positionX(x), positionZ(z)
{
}

void BaggageTug::Draw() const
{
    const float firstCartZ = positionZ - 5.25f;
    const float secondCartZ = positionZ - 10.6f;
    const float thirdCartZ = positionZ - 15.95f;

    DrawTug(positionX, positionZ);
    DrawLuggageCart(positionX, firstCartZ, Color{65, 101, 131, 255}, Color{174, 140, 93, 255});
    DrawLuggageCart(positionX, secondCartZ, Color{105, 119, 117, 255}, Color{65, 101, 131, 255});
    DrawLuggageCart(positionX, thirdCartZ, Color{174, 140, 93, 255}, Color{105, 119, 117, 255});
    ConnectCarts(positionX, firstCartZ, secondCartZ);
    ConnectCarts(positionX, secondCartZ, thirdCartZ);
}
