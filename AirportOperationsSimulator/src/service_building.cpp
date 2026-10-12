#include "service_building.h"

#include "raylib.h"

namespace
{
    constexpr float BuildingLeft = 165.0f;
    constexpr float BuildingScale = 1.14f;
    constexpr float GroundWidth = 66.0f;
    constexpr float GroundDepth = 46.0f;
    constexpr float BuildingCenterZ = -5.0f;
    constexpr float GroundHeight = 12.0f;
    constexpr float UpperWidth = 40.0f;
    constexpr float UpperDepth = 31.0f;
    constexpr float UpperHeight = 8.0f;

    constexpr Color Foundation{103, 111, 113, 255};
    constexpr Color GroundWall{196, 190, 169, 255};
    constexpr Color UpperWall{216, 212, 197, 255};
    constexpr Color WallShade{160, 169, 166, 255};
    constexpr Color Roof{57, 78, 91, 255};
    constexpr Color RoofEdge{92, 111, 117, 255};
    constexpr Color Glass{54, 117, 143, 255};
    constexpr Color GlassHighlight{131, 177, 186, 255};
    constexpr Color Trim{226, 218, 190, 255};

    void DrawBuildingCube(Vector3 center, Vector3 size, Color color)
    {
        center = {
            BuildingLeft + (center.x - BuildingLeft) * BuildingScale,
            center.y * BuildingScale,
            BuildingCenterZ + (center.z - BuildingCenterZ) * BuildingScale
        };
        size = {
            size.x * BuildingScale,
            size.y * BuildingScale,
            size.z * BuildingScale
        };
        DrawCubeV(center, size, color);
    }

    void DrawFrontWindow(float x, float y, float width, float height, float frontZ)
    {
        DrawBuildingCube(
            {x, y, frontZ - 0.11f},
            {width + 0.8f, height + 0.8f, 0.32f},
            Trim);
        DrawBuildingCube(
            {x, y, frontZ - 0.30f},
            {width, height, 0.12f},
            Glass);
        DrawBuildingCube(
            {x, y + height * 0.28f, frontZ - 0.38f},
            {width * 0.88f, 0.12f, 0.04f},
            GlassHighlight);
        DrawBuildingCube(
            {x, y, frontZ - 0.39f},
            {0.08f, height, 0.04f},
            Trim);
    }

    void DrawSideWindow(float x, float y, float z, float width, float height)
    {
        DrawBuildingCube(
            {x + 0.10f, y, z},
            {0.32f, height + 0.8f, width + 0.8f},
            Trim);
        DrawBuildingCube(
            {x + 0.29f, y, z},
            {0.12f, height, width},
            Glass);
        DrawBuildingCube(
            {x + 0.37f, y + height * 0.28f, z},
            {0.04f, 0.12f, width * 0.88f},
            GlassHighlight);
    }

    void DrawGroundFloor()
    {
        const float centerX = BuildingLeft + GroundWidth * 0.5f;
        const float frontZ = BuildingCenterZ - GroundDepth * 0.5f;

        DrawBuildingCube(
            {centerX, 0.2f, BuildingCenterZ},
            {GroundWidth + 3.0f, 0.4f, GroundDepth + 3.0f},
            Foundation);
        DrawBuildingCube(
            {centerX, GroundHeight * 0.5f, BuildingCenterZ},
            {GroundWidth, GroundHeight, GroundDepth},
            GroundWall);
        DrawBuildingCube(
            {centerX, 6.0f, BuildingCenterZ + GroundDepth * 0.5f + 0.12f},
            {GroundWidth - 2.0f, 0.55f, 0.3f},
            WallShade);

        DrawFrontWindow(centerX - 20.0f, 6.7f, 8.0f, 5.6f, frontZ);
        DrawFrontWindow(centerX + 1.0f, 6.7f, 8.0f, 5.6f, frontZ);
        DrawFrontWindow(centerX + 22.0f, 6.7f, 8.0f, 5.6f, frontZ);

        DrawSideWindow(
            BuildingLeft + GroundWidth,
            6.6f,
            BuildingCenterZ - 10.0f,
            9.0f,
            5.6f);
        DrawSideWindow(
            BuildingLeft + GroundWidth,
            6.6f,
            BuildingCenterZ + 10.0f,
            9.0f,
            5.6f);
    }

    void DrawUpperFloor()
    {
        const float upperCenterX = BuildingLeft + UpperWidth * 0.5f;
        const float upperCenterZ = BuildingCenterZ;
        const float frontZ = upperCenterZ - UpperDepth * 0.5f;
        const float wallCenterY = GroundHeight + UpperHeight * 0.5f + 0.8f;

        DrawBuildingCube(
            {upperCenterX, wallCenterY, upperCenterZ},
            {UpperWidth, UpperHeight, UpperDepth},
            UpperWall);
        DrawBuildingCube(
            {upperCenterX, GroundHeight + 1.05f, upperCenterZ},
            {UpperWidth + 1.5f, 0.5f, UpperDepth + 1.5f},
            RoofEdge);

        DrawFrontWindow(
            upperCenterX - 10.0f,
            GroundHeight + 5.1f,
            8.0f,
            3.8f,
            frontZ);
        DrawFrontWindow(
            upperCenterX + 10.0f,
            GroundHeight + 5.1f,
            8.0f,
            3.8f,
            frontZ);
        DrawSideWindow(
            BuildingLeft + UpperWidth,
            GroundHeight + 5.0f,
            upperCenterZ,
            11.0f,
            3.8f);

        const float roofY = GroundHeight + UpperHeight + 1.7f;
        DrawBuildingCube(
            {upperCenterX, roofY - 0.4f, upperCenterZ},
            {UpperWidth + 3.0f, 0.8f, UpperDepth + 3.0f},
            RoofEdge);
        DrawBuildingCube(
            {upperCenterX, roofY + 0.12f, upperCenterZ},
            {UpperWidth + 1.5f, 0.35f, UpperDepth + 1.5f},
            Roof);
    }

    void DrawExposedRoof()
    {
        const float groundCenterX = BuildingLeft + GroundWidth * 0.5f;
        const float upperRight = BuildingLeft + UpperWidth;
        const float groundRight = BuildingLeft + GroundWidth;
        const float ledgeWidth = groundRight - upperRight;
        const float roofY = GroundHeight + 0.72f;

        DrawBuildingCube(
            {groundCenterX, roofY, BuildingCenterZ},
            {GroundWidth + 2.0f, 0.9f, GroundDepth + 2.0f},
            RoofEdge);
        DrawBuildingCube(
            {upperRight + ledgeWidth * 0.5f, roofY + 0.48f, BuildingCenterZ},
            {ledgeWidth, 0.12f, GroundDepth - 1.0f},
            Color{177, 184, 178, 255});
    }
}

void ServiceBuilding::Draw() const
{
    DrawGroundFloor();
    DrawExposedRoof();
    DrawUpperFloor();
}
