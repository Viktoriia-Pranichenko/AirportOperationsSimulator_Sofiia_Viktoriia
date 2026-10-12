#include "airport_service_vehicles.h"

#include "raylib.h"

#include <initializer_list>

namespace
{
    constexpr Color Tire{37, 42, 43, 255};
    constexpr Color WheelHub{162, 166, 155, 255};
    constexpr Color Glass{42, 100, 125, 255};
    constexpr Color Trim{195, 201, 192, 255};
    constexpr Color FollowYellow{226, 181, 48, 255};
    constexpr Color FollowBlack{39, 43, 43, 255};
    constexpr Color TugYellow{221, 177, 55, 255};
    constexpr Color TugShade{153, 123, 49, 255};
    constexpr Color LoaderWhite{210, 211, 198, 255};
    constexpr Color LoaderShade{143, 155, 153, 255};
    constexpr Color Belt{61, 70, 69, 255};

    void DrawWheel(float x, float y, float z, float radius)
    {
        DrawCylinderEx(
            {x - 0.14f, y, z},
            {x + 0.14f, y, z},
            radius,
            radius,
            10,
            Tire);
        DrawCylinderEx(
            {x - 0.16f, y, z},
            {x - 0.07f, y, z},
            radius * 0.46f,
            radius * 0.46f,
            8,
            WheelHub);
    }

    void DrawRampSurface(Vector3 lowerLeft, Vector3 lowerRight,
        Vector3 upperRight, Vector3 upperLeft)
    {
        DrawTriangle3D(lowerLeft, lowerRight, upperRight, Belt);
        DrawTriangle3D(lowerLeft, upperRight, upperLeft, Belt);
        DrawCylinderEx(lowerLeft, upperLeft, 0.09f, 0.09f, 8, Trim);
        DrawCylinderEx(lowerRight, upperRight, 0.09f, 0.09f, 8, Trim);
    }
}

FollowMeCar::FollowMeCar(float positionX, float positionZ)
    : positionX(positionX), positionZ(positionZ)
{
}

void FollowMeCar::Draw() const
{
    const float x = positionX;
    const float z = positionZ;

    DrawCubeV({x, 0.72f, z}, {2.08f, 0.48f, 4.55f}, FollowBlack);
    DrawCubeV({x, 1.08f, z + 0.12f}, {2.0f, 0.78f, 3.58f}, FollowYellow);
    DrawCubeV({x, 1.53f, z - 0.35f}, {1.84f, 0.86f, 1.82f}, FollowYellow);
    DrawCubeV({x, 1.98f, z - 0.35f}, {1.9f, 0.12f, 1.9f}, FollowBlack);

    DrawCubeV({x, 1.57f, z + 0.59f}, {1.5f, 0.55f, 0.05f}, Glass);
    DrawCubeV({x - 0.94f, 1.5f, z - 0.35f}, {0.05f, 0.5f, 0.94f}, Glass);
    DrawCubeV({x + 0.94f, 1.5f, z - 0.35f}, {0.05f, 0.5f, 0.94f}, Glass);

    for (float side : {-1.0f, 1.0f})
    {
        for (float stripeZ : {-1.65f, -0.85f, 0.0f, 0.85f, 1.65f})
        {
            DrawCubeV(
                {x + side * 1.015f, 0.99f, z + stripeZ},
                {0.035f, 0.28f, 0.46f},
                FollowBlack);
        }

        for (float axleZ : {-1.48f, 1.48f})
            DrawWheel(x + side * 1.04f, 0.38f, z + axleZ, 0.38f);
    }

    DrawCubeV({x, 2.12f, z - 0.35f}, {0.32f, 0.08f, 0.3f}, FollowYellow);
    DrawCubeV({x, 2.27f, z - 0.35f}, {0.24f, 0.2f, 0.22f}, Color{220, 52, 37, 255});
    DrawCubeV({x - 0.68f, 1.14f, z + 2.34f}, {0.32f, 0.16f, 0.06f}, Trim);
    DrawCubeV({x + 0.68f, 1.14f, z + 2.34f}, {0.32f, 0.16f, 0.06f}, Trim);
}

AircraftPushbackTug::AircraftPushbackTug(float positionX, float positionZ)
    : positionX(positionX), positionZ(positionZ)
{
}

void AircraftPushbackTug::Draw() const
{
    const float x = positionX;
    const float z = positionZ;

    DrawCubeV({x, 0.82f, z}, {2.65f, 0.82f, 5.75f}, TugShade);
    DrawCubeV({x, 1.26f, z + 0.62f}, {2.58f, 0.68f, 3.45f}, TugYellow);
    DrawCubeV({x, 1.78f, z - 0.9f}, {2.12f, 1.15f, 1.8f}, TugYellow);
    DrawCubeV({x, 2.37f, z - 0.9f}, {2.25f, 0.14f, 1.92f}, TugShade);

    DrawCubeV({x, 1.83f, z - 0.02f}, {1.58f, 0.7f, 0.06f}, Glass);
    DrawCubeV({x - 1.09f, 1.8f, z - 0.9f}, {0.06f, 0.64f, 0.92f}, Glass);
    DrawCubeV({x + 1.09f, 1.8f, z - 0.9f}, {0.06f, 0.64f, 0.92f}, Glass);

    for (float side : {-1.0f, 1.0f})
    {
        for (float axleZ : {-1.85f, 1.82f})
            DrawWheel(x + side * 1.34f, 0.48f, z + axleZ, 0.48f);
    }

    DrawCubeV({x, 0.61f, z + 3.02f}, {1.8f, 0.25f, 0.3f}, Trim);
    DrawCubeV({x, 0.7f, z + 3.37f}, {1.12f, 0.34f, 0.52f}, TugYellow);
    DrawCubeV({x, 0.58f, z - 3.02f}, {1.25f, 0.18f, 0.38f}, TugShade);
    DrawCubeV({x - 0.72f, 1.16f, z + 2.94f}, {0.28f, 0.16f, 0.06f}, Trim);
    DrawCubeV({x + 0.72f, 1.16f, z + 2.94f}, {0.28f, 0.16f, 0.06f}, Trim);
}

BaggageBeltLoader::BaggageBeltLoader(float positionX, float positionZ)
    : positionX(positionX), positionZ(positionZ)
{
}

void BaggageBeltLoader::Draw() const
{
    const float x = positionX;
    const float z = positionZ;

    DrawCubeV({x, 0.76f, z + 0.25f}, {2.35f, 0.62f, 4.8f}, LoaderShade);
    DrawCubeV({x, 1.12f, z + 0.25f}, {2.28f, 0.2f, 4.45f}, LoaderWhite);
    DrawCubeV({x, 1.57f, z + 1.13f}, {1.82f, 0.86f, 1.7f}, LoaderWhite);
    DrawCubeV({x, 2.02f, z + 1.13f}, {1.9f, 0.12f, 1.8f}, LoaderShade);
    DrawCubeV({x, 1.62f, z + 2.0f}, {1.45f, 0.48f, 0.05f}, Glass);

    for (float side : {-1.0f, 1.0f})
    {
        for (float axleZ : {-1.48f, 1.75f})
            DrawWheel(x + side * 1.18f, 0.37f, z + axleZ, 0.37f);
    }

    const Vector3 beltLowerLeft{x - 0.82f, 1.18f, z - 0.65f};
    const Vector3 beltLowerRight{x + 0.82f, 1.18f, z - 0.65f};
    const Vector3 beltUpperRight{x + 0.82f, 2.56f, z - 4.05f};
    const Vector3 beltUpperLeft{x - 0.82f, 2.56f, z - 4.05f};
    DrawRampSurface(beltLowerLeft, beltLowerRight, beltUpperRight, beltUpperLeft);

    DrawCylinderEx(
        {x - 0.52f, 1.12f, z + 0.5f},
        {x - 0.52f, 2.15f, z - 2.03f},
        0.08f,
        0.08f,
        8,
        LoaderShade);
    DrawCylinderEx(
        {x + 0.52f, 1.12f, z + 0.5f},
        {x + 0.52f, 2.15f, z - 2.03f},
        0.08f,
        0.08f,
        8,
        LoaderShade);
}
