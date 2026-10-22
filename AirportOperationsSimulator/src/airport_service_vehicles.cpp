#include "airport_service_vehicles.h"

#include "airport_terminal.h"
#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace
{
    constexpr Color Tire{37, 42, 43, 255};
    constexpr Color WheelHub{162, 166, 155, 255};
    constexpr Color Glass{42, 100, 125, 255};
    constexpr Color Trim{195, 201, 192, 255};
    constexpr Color TugYellow{221, 177, 55, 255};
    constexpr Color TugShade{153, 123, 49, 255};
    constexpr Color LoaderWhite{210, 211, 198, 255};
    constexpr Color LoaderShade{143, 155, 153, 255};
    constexpr Color Belt{61, 70, 69, 255};
    constexpr Color CateringWhite{218, 220, 207, 255};
    constexpr Color CateringAccent{205, 112, 46, 255};
    constexpr Color TankBlue{75, 143, 166, 255};
    constexpr Color TankShade{47, 105, 128, 255};
    constexpr Color UtilityGreen{91, 126, 91, 255};
    constexpr Color StairYellow{213, 174, 72, 255};
    constexpr Color StairMetal{174, 183, 177, 255};
    constexpr Color TankerBody{201, 207, 195, 255};
    constexpr Color TankerAccent{218, 171, 55, 255};
    constexpr Color FuelTank{202, 207, 193, 255};
    constexpr Color FuelTankShade{152, 165, 161, 255};

    class ServiceVehicleDrawTransform
    {
    public:
        ServiceVehicleDrawTransform(float x, float z, float headingDegrees)
        {
            rlPushMatrix();
            rlTranslatef(x, 0.0f, z);
            rlRotatef(headingDegrees, 0.0f, 1.0f, 0.0f);
            rlTranslatef(-x, 0.0f, -z);
        }

        ~ServiceVehicleDrawTransform()
        {
            rlPopMatrix();
        }
    };

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

ServiceVehiclePosition::ServiceVehiclePosition(float positionX, float positionZ)
    : positionX(positionX), positionZ(positionZ)
{
}

void ServiceVehiclePosition::SetPosition(float positionX, float positionZ)
{
    this->positionX = positionX;
    this->positionZ = positionZ;
}

void ServiceVehiclePosition::SetHeadingDegrees(float headingDegrees)
{
    this->headingDegrees = headingDegrees;
}

Vector2 ServiceVehiclePosition::GetPosition() const
{
    return {positionX, positionZ};
}

bool ServiceVehiclePosition::MoveTowardsGate(
    int gateNumber, const AirportTerminal& terminal, float deltaTime)
{
    if (gateNumber < 1 || deltaTime <= 0.0f) return false;

    const std::string gateId = "GATE_" + std::to_string(gateNumber);
    const AirportParkingStand* targetGate = nullptr;
    for (const AirportParkingStand& stand : terminal.GetParkingStands())
    {
        if (stand.id == gateId)
        {
            targetGate = &stand;
            break;
        }
    }
    if (targetGate == nullptr) return false;

    constexpr float MovementSpeed = 12.0f;
    const float dx = targetGate->position.x - positionX;
    const float dz = targetGate->position.z - positionZ;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance <= 0.001f) return true;

    const float travelDistance = std::min(distance, MovementSpeed * deltaTime);
    positionX += dx / distance * travelDistance;
    positionZ += dz / distance * travelDistance;
    return travelDistance >= distance;
}

AircraftPushbackTug::AircraftPushbackTug(float positionX, float positionZ)
    : ServiceVehiclePosition(positionX, positionZ)
{
}

void AircraftPushbackTug::Draw() const
{
    const float x = positionX;
    const float z = positionZ;
    const ServiceVehicleDrawTransform transform(x, z, headingDegrees);

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
    : ServiceVehiclePosition(positionX, positionZ)
{
}

void BaggageBeltLoader::Draw() const
{
    const float x = positionX;
    const float z = positionZ;
    const ServiceVehicleDrawTransform transform(x, z, headingDegrees);

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

CateringTruck::CateringTruck(float positionX, float positionZ)
    : ServiceVehiclePosition(positionX, positionZ)
{
}

void CateringTruck::Draw() const
{
    const float x = positionX;
    const float z = positionZ;
    const ServiceVehicleDrawTransform transform(x, z, headingDegrees);

    DrawCubeV({x, 0.69f, z}, {2.55f, 0.45f, 7.7f}, CateringAccent);
    DrawCubeV({x, 1.93f, z - 0.72f}, {2.52f, 2.12f, 4.6f}, CateringWhite);
    DrawCubeV({x, 2.03f, z - 0.72f}, {2.58f, 0.2f, 4.72f}, LoaderShade);
    DrawCubeV({x, 1.55f, z - 0.72f}, {2.58f, 0.28f, 4.72f}, CateringAccent);

    DrawCubeV({x, 1.53f, z + 2.42f}, {2.46f, 1.25f, 2.2f}, CateringWhite);
    DrawCubeV({x, 2.09f, z + 2.42f}, {2.32f, 0.77f, 0.06f}, Glass);
    DrawCubeV({x - 1.26f, 1.83f, z + 2.37f}, {0.055f, 0.72f, 0.9f}, Glass);
    DrawCubeV({x + 1.26f, 1.83f, z + 2.37f}, {0.055f, 0.72f, 0.9f}, Glass);
    DrawCubeV({x, 0.68f, z + 3.98f}, {2.72f, 0.24f, 0.25f}, Trim);

    for (float side : {-1.0f, 1.0f})
    {
        for (float axleZ : {-2.55f, 2.65f})
            DrawWheel(x + side * 1.31f, 0.42f, z + axleZ, 0.4f);
        DrawCubeV(
            {x + side * 1.29f, 1.25f, z - 0.72f},
            {0.045f, 0.1f, 3.75f},
            CateringAccent);
    }
}

WaterServiceTruck::WaterServiceTruck(float positionX, float positionZ)
    : ServiceVehiclePosition(positionX, positionZ)
{
}

void WaterServiceTruck::Draw() const
{
    const float x = positionX;
    const float z = positionZ;
    const ServiceVehicleDrawTransform transform(x, z, headingDegrees);

    DrawCubeV({x, 0.69f, z}, {2.5f, 0.45f, 8.25f}, LoaderShade);
    DrawCubeV({x, 1.48f, z + 2.75f}, {2.42f, 1.6f, 2.15f}, CateringWhite);
    DrawCubeV({x, 2.05f, z + 2.75f}, {2.3f, 0.77f, 0.06f}, Glass);
    DrawCubeV({x - 1.22f, 1.91f, z + 2.72f}, {0.055f, 0.75f, 0.95f}, Glass);
    DrawCubeV({x + 1.22f, 1.91f, z + 2.72f}, {0.055f, 0.75f, 0.95f}, Glass);

    DrawCylinderEx(
        {x, 1.26f, z - 2.65f},
        {x, 1.26f, z + 1.05f},
        1.05f,
        1.05f,
        16,
        TankBlue);
    DrawCylinderEx(
        {x - 0.04f, 1.26f, z - 2.67f},
        {x + 0.04f, 1.26f, z - 2.67f},
        1.0f,
        1.0f,
        12,
        TankShade);
    DrawCylinderEx(
        {x - 0.04f, 1.26f, z + 1.07f},
        {x + 0.04f, 1.26f, z + 1.07f},
        1.0f,
        1.0f,
        12,
        TankShade);
    DrawCubeV({x, 2.34f, z - 0.8f}, {0.42f, 0.12f, 0.42f}, Trim);
    DrawCubeV({x, 0.7f, z + 3.91f}, {2.7f, 0.25f, 0.24f}, Trim);

    for (float side : {-1.0f, 1.0f})
    {
        for (float axleZ : {-2.68f, 2.83f})
            DrawWheel(x + side * 1.3f, 0.42f, z + axleZ, 0.4f);
        DrawCubeV(
            {x + side * 0.83f, 2.33f, z - 0.8f},
            {0.08f, 0.13f, 3.4f},
            Trim);
    }
}

MaintenancePickupTruck::MaintenancePickupTruck(float positionX, float positionZ)
    : ServiceVehiclePosition(positionX, positionZ)
{
}

void MaintenancePickupTruck::Draw() const
{
    const float x = positionX;
    const float z = positionZ;
    const ServiceVehicleDrawTransform transform(x, z, headingDegrees);

    DrawCubeV({x, 0.67f, z}, {2.15f, 0.4f, 4.95f}, UtilityGreen);
    DrawCubeV({x, 1.03f, z - 1.1f}, {2.0f, 0.45f, 2.15f}, CateringAccent);
    DrawCubeV({x, 1.52f, z + 1.0f}, {1.95f, 1.35f, 2.05f}, UtilityGreen);
    DrawCubeV({x, 2.14f, z + 1.0f}, {1.88f, 0.78f, 0.055f}, Glass);
    DrawCubeV({x - 0.99f, 1.87f, z + 1.0f}, {0.055f, 0.72f, 0.94f}, Glass);
    DrawCubeV({x + 0.99f, 1.87f, z + 1.0f}, {0.055f, 0.72f, 0.94f}, Glass);
    DrawCubeV({x, 1.14f, z - 1.13f}, {1.88f, 0.08f, 1.7f}, Trim);
    DrawCubeV({x, 0.72f, z + 2.58f}, {2.3f, 0.24f, 0.24f}, Trim);

    for (float side : {-1.0f, 1.0f})
    {
        for (float axleZ : {-1.58f, 1.64f})
            DrawWheel(x + side * 1.08f, 0.39f, z + axleZ, 0.37f);
        DrawCubeV(
            {x + side * 1.09f, 0.97f, z - 1.1f},
            {0.05f, 0.12f, 1.9f},
            CateringAccent);
    }
}

PassengerBoardingStairs::PassengerBoardingStairs(float positionX, float positionZ)
    : ServiceVehiclePosition(positionX, positionZ)
{
}

void PassengerBoardingStairs::Draw() const
{
    const float x = positionX;
    const float z = positionZ;
    const ServiceVehicleDrawTransform transform(x, z, headingDegrees);

    DrawCubeV({x, 0.62f, z + 0.05f}, {2.45f, 0.38f, 4.75f}, StairYellow);
    DrawCubeV({x, 0.87f, z + 1.72f}, {2.25f, 0.16f, 1.12f}, StairMetal);
    DrawCubeV({x, 0.87f, z - 1.66f}, {2.25f, 0.16f, 1.05f}, StairMetal);
    DrawCubeV({x, 2.55f, z - 1.82f}, {2.35f, 0.2f, 1.2f}, StairYellow);

    for (float side : {-1.0f, 1.0f})
    {
        for (float axleZ : {-1.62f, 1.62f})
            DrawWheel(x + side * 1.18f, 0.36f, z + axleZ, 0.34f);
    }

    constexpr int StepCount = 8;
    const float firstStepZ = z + 1.42f;
    const float lastStepZ = z - 1.53f;
    for (int step = 0; step < StepCount; ++step)
    {
        const float fraction = static_cast<float>(step) / (StepCount - 1);
        const float stepZ = firstStepZ + (lastStepZ - firstStepZ) * fraction;
        const float stepY = 1.13f + 1.25f * fraction;
        DrawCubeV(
            {x, stepY, stepZ},
            {1.52f, 0.15f, 0.38f},
            StairMetal);
    }

    const Vector3 lowerLeft{x - 0.94f, 1.08f, firstStepZ + 0.22f};
    const Vector3 upperLeft{x - 0.94f, 2.67f, lastStepZ - 0.25f};
    const Vector3 lowerRight{x + 0.94f, 1.08f, firstStepZ + 0.22f};
    const Vector3 upperRight{x + 0.94f, 2.67f, lastStepZ - 0.25f};
    DrawCylinderEx(lowerLeft, upperLeft, 0.075f, 0.075f, 8, StairYellow);
    DrawCylinderEx(lowerRight, upperRight, 0.075f, 0.075f, 8, StairYellow);

    for (int post = 0; post < 4; ++post)
    {
        const float fraction = static_cast<float>(post) / 3.0f;
        const float postZ = lowerLeft.z + (upperLeft.z - lowerLeft.z) * fraction;
        const float postY = lowerLeft.y + (upperLeft.y - lowerLeft.y) * fraction;
        for (float side : {-1.0f, 1.0f})
        {
            DrawCylinderEx(
                {x + side * 0.94f, postY - 0.12f, postZ},
                {x + side * 0.94f, postY + 0.16f, postZ},
                0.045f,
                0.045f,
                6,
                StairMetal);
        }
    }
}

AircraftRefuellingTanker::AircraftRefuellingTanker(float positionX, float positionZ)
    : ServiceVehiclePosition(positionX, positionZ)
{
}

void AircraftRefuellingTanker::Draw() const
{
    const float x = positionX;
    const float z = positionZ;
    const ServiceVehicleDrawTransform transform(x, z, headingDegrees);

    DrawCubeV({x, 0.72f, z}, {2.55f, 0.44f, 9.15f}, LoaderShade);
    DrawCubeV({x, 1.48f, z + 2.72f}, {2.48f, 1.55f, 2.3f}, TankerBody);
    DrawCubeV({x, 2.02f, z + 2.72f}, {2.32f, 0.76f, 0.06f}, Glass);
    DrawCubeV({x - 1.25f, 1.9f, z + 2.7f}, {0.055f, 0.72f, 0.96f}, Glass);
    DrawCubeV({x + 1.25f, 1.9f, z + 2.7f}, {0.055f, 0.72f, 0.96f}, Glass);

    DrawCylinderEx(
        {x, 1.5f, z - 3.08f},
        {x, 1.5f, z + 0.92f},
        1.04f,
        1.04f,
        16,
        FuelTank);
    DrawCylinderEx(
        {x - 0.04f, 1.5f, z - 3.1f},
        {x + 0.04f, 1.5f, z - 3.1f},
        0.98f,
        0.98f,
        12,
        FuelTankShade);
    DrawCylinderEx(
        {x - 0.04f, 1.5f, z + 0.94f},
        {x + 0.04f, 1.5f, z + 0.94f},
        0.98f,
        0.98f,
        12,
        FuelTankShade);
    DrawCubeV({x, 2.59f, z - 1.45f}, {0.42f, 0.12f, 0.42f}, TankerAccent);
    DrawCubeV({x, 0.7f, z + 3.93f}, {2.72f, 0.25f, 0.24f}, Trim);

    for (float side : {-1.0f, 1.0f})
    {
        for (float axleZ : {-2.9f, 2.85f})
            DrawWheel(x + side * 1.32f, 0.42f, z + axleZ, 0.4f);

        DrawCubeV(
            {x + side * 0.91f, 1.48f, z - 1.45f},
            {0.07f, 0.18f, 2.55f},
            TankerAccent);
        DrawCubeV(
            {x + side * 1.29f, 1.13f, z - 3.05f},
            {0.05f, 0.1f, 0.2f},
            TankerAccent);
    }
}
