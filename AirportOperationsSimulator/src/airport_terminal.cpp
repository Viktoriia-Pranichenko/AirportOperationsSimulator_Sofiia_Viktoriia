#include "airport_terminal.h"

#include "raylib.h"

#include <cmath>
#include <cstdlib>

namespace
{
    Vector3 Normalize(Vector3 vector)
    {
        const float length = std::sqrt(
            vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
        return length > 0.0f
            ? Vector3{vector.x / length, vector.y / length, vector.z / length}
            : Vector3{0.0f, 0.0f, 0.0f};
    }

    Vector3 CrossProduct(Vector3 left, Vector3 right)
    {
        return {
            left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x
        };
    }

    Ray GetScreenRay(Vector2 screenPosition, const Camera3D& camera)
    {
        const Vector3 forward = Normalize({
            camera.target.x - camera.position.x,
            camera.target.y - camera.position.y,
            camera.target.z - camera.position.z
        });
        const Vector3 right = Normalize(CrossProduct(forward, camera.up));
        const Vector3 cameraUp = CrossProduct(right, forward);
        const float screenWidth = static_cast<float>(GetScreenWidth());
        const float screenHeight = static_cast<float>(GetScreenHeight());
        if (screenWidth <= 0.0f || screenHeight <= 0.0f)
            return {camera.position, forward};

        const float normalizedX = 2.0f * screenPosition.x / screenWidth - 1.0f;
        const float normalizedY = 1.0f - 2.0f * screenPosition.y / screenHeight;
        const float halfFovTangent = std::tan(camera.fovy * DEG2RAD * 0.5f);
        const float aspectRatio = screenWidth / screenHeight;
        const Vector3 direction = Normalize({
            forward.x + right.x * normalizedX * halfFovTangent * aspectRatio +
                cameraUp.x * normalizedY * halfFovTangent,
            forward.y + right.y * normalizedX * halfFovTangent * aspectRatio +
                cameraUp.y * normalizedY * halfFovTangent,
            forward.z + right.z * normalizedX * halfFovTangent * aspectRatio +
                cameraUp.z * normalizedY * halfFovTangent
        });
        return {camera.position, direction};
    }

    constexpr Color Wall{211, 204, 177, 255};
    constexpr Color WallSide{164, 180, 175, 255};
    constexpr Color Roof{43, 73, 94, 255};
    constexpr Color Trim{91, 127, 140, 255};
    constexpr Color Glass{35, 104, 134, 255};
    constexpr Color GlassHighlight{136, 192, 199, 255};
    constexpr Color DoorFrame{226, 216, 187, 255};
    constexpr float WallHeight = 12.0f;
    constexpr float RoofY = 12.65f;
    constexpr float BackFacadeZ = -8.0f;
    constexpr float MainHallRearZ = 57.0f;
    constexpr float TerminalZOffset = 8.0f;
    constexpr float LeftInnerX = -56.0f;
    constexpr float RightInnerX = 56.0f;
    constexpr float SideWingCenterX = 65.0f;
    constexpr float SideWingCenterZ = -68.0f;
    constexpr float SideWingLength = 160.0f;
    constexpr float TerminalScale = 1.25f;
    constexpr float SideStandCenterX = 42.0f;
    constexpr Color BoardingDoorGlass{190, 105, 58, 255};
    constexpr Color BoardingDoorHighlight{239, 184, 119, 255};

    void Box(Vector3 center, Vector3 size, Color color)
    {
        center.x *= TerminalScale;
        center.y *= TerminalScale;
        center.z *= TerminalScale;
        center.z += TerminalZOffset;
        size.x *= TerminalScale;
        size.y *= TerminalScale;
        size.z *= TerminalScale;
        DrawCubeV(center, size, color);
    }

    void DrawBackDoor(float x)
    {
        Box({x, 2.45f, BackFacadeZ - 0.16f}, {4.6f, 4.9f, 0.34f}, DoorFrame);
        Box({x, 2.4f, BackFacadeZ - 0.36f}, {3.5f, 4.2f, 0.12f}, BoardingDoorGlass);
        Box({x, 3.0f, BackFacadeZ - 0.44f}, {0.09f, 0.14f, 0.035f}, BoardingDoorHighlight);
        // Recessed portal piers and a projecting canopy mark this as a passenger gate.
        Box({x - 2.85f, 2.55f, BackFacadeZ - 1.15f}, {0.5f, 4.8f, 2.4f}, WallSide);
        Box({x + 2.85f, 2.55f, BackFacadeZ - 1.15f}, {0.5f, 4.8f, 2.4f}, WallSide);
        Box({x, 5.05f, BackFacadeZ - 1.55f}, {7.2f, 0.42f, 3.2f}, Trim);
    }

    void DrawBackWindow(float x, float y, float width, float height)
    {
        Box({x, y, BackFacadeZ - 0.13f}, {width, height, 0.18f}, Glass);
        Box({x, y + height * 0.30f, BackFacadeZ - 0.24f}, {width * 0.92f, 0.12f, 0.035f}, GlassHighlight);
        Box({x - width * 0.48f, y, BackFacadeZ - 0.25f}, {0.07f, height + 0.12f, 0.035f}, DoorFrame);
    }

    void DrawMainHallWindow(float x, float y)
    {
        Box({x, y, MainHallRearZ + 0.12f}, {10.0f, 3.2f, 0.18f}, Glass);
        Box({x, y + 0.95f, MainHallRearZ + 0.23f}, {9.2f, 0.12f, 0.035f}, GlassHighlight);
        Box({x - 4.8f, y, MainHallRearZ + 0.24f}, {0.07f, 3.32f, 0.035f}, DoorFrame);
    }

    void DrawSideDoor(float x, float z)
    {
        Box({x, 2.45f, z}, {0.34f, 4.9f, 4.6f}, DoorFrame);
        Box({x, 2.4f, z}, {0.12f, 4.2f, 3.5f}, BoardingDoorGlass);
        Box({x, 3.0f, z}, {0.035f, 0.14f, 0.09f}, BoardingDoorHighlight);
        const float canopyX = (x < 0.0f) ? x + 0.55f : x - 0.55f;
        const float outerX = (x < 0.0f) ? x + 1.25f : x - 1.25f;
        Box({outerX, 2.55f, z - 2.85f}, {2.4f, 4.8f, 0.5f}, WallSide);
        Box({outerX, 2.55f, z + 2.85f}, {2.4f, 4.8f, 0.5f}, WallSide);
        Box({canopyX, 5.05f, z}, {3.1f, 0.42f, 7.2f}, Trim);
    }

    void DrawSideWindow(float x, float z, float y, float width, float height)
    {
        Box({x, y, z}, {0.18f, height, width}, Glass);
        Box({x, y + height * 0.30f, z}, {0.035f, 0.12f, width * 0.92f}, GlassHighlight);
        const float frameX = (x < 0.0f) ? x + 0.12f : x - 0.12f;
        Box({frameX, y, z - width * 0.48f}, {0.035f, height + 0.12f, 0.07f}, DoorFrame);
    }

    void DrawTerminal()
    {
        // Three connected, two-storey blocks make a U around the open central apron.
        Box({0.0f, WallHeight * 0.5f, 0.0f}, {176.0f, WallHeight, 16.0f}, Wall);
        Box({-SideWingCenterX, WallHeight * 0.5f, SideWingCenterZ},
            {18.0f, WallHeight, SideWingLength}, WallSide);
        Box({SideWingCenterX, WallHeight * 0.5f, SideWingCenterZ},
            {18.0f, WallHeight, SideWingLength}, WallSide);
        // A broader central terminal hall continues behind the U-shaped boarding concourse.
        Box({0.0f, WallHeight * 0.5f, 31.0f}, {176.0f, WallHeight, 52.0f}, WallSide);

        // Roof slabs and fascia read clearly above the upper-storey glazing.
        Box({0.0f, RoofY, 0.0f}, {178.0f, 1.3f, 18.0f}, Roof);
        Box({-SideWingCenterX, RoofY, SideWingCenterZ},
            {20.0f, 1.3f, SideWingLength + 2.0f}, Roof);
        Box({SideWingCenterX, RoofY, SideWingCenterZ},
            {20.0f, 1.3f, SideWingLength + 2.0f}, Roof);
        Box({0.0f, RoofY, 31.0f}, {178.0f, 1.3f, 54.0f}, Roof);

        // The floor band wraps the apron-facing sides and makes the two levels distinct.
        Box({0.0f, 6.0f, BackFacadeZ - 0.12f}, {176.0f, 0.55f, 0.38f}, Trim);
        Box({LeftInnerX + 0.14f, 6.0f, SideWingCenterZ}, {0.38f, 0.55f, SideWingLength}, Trim);
        Box({RightInnerX - 0.14f, 6.0f, SideWingCenterZ}, {0.38f, 0.55f, SideWingLength}, Trim);

        // Four evenly spaced boarding doors line the main apron-facing section.
        constexpr float backDoorXs[] = {-27.0f, -9.0f, 9.0f, 27.0f};
        for (float x : backDoorXs) DrawBackDoor(x);
        constexpr float backGroundWindowXs[] = {-70.0f, -50.0f, -18.0f, 0.0f, 18.0f, 50.0f, 70.0f};
        for (float x : backGroundWindowXs)
            DrawBackWindow(x, 2.55f, 5.0f, 3.2f);
        constexpr float backUpperWindowXs[] = {-76.0f, -56.0f, -36.0f, -18.0f, 0.0f, 18.0f, 36.0f, 56.0f, 76.0f};
        for (float x : backUpperWindowXs)
        {
            DrawBackWindow(x, 9.0f, 7.0f, 3.2f);
        }
        Box({0.0f, 6.0f, MainHallRearZ + 0.05f}, {172.0f, 0.55f, 0.38f}, Trim);
        constexpr float mainHallWindowXs[] = {-72.0f, -48.0f, -24.0f, 0.0f, 24.0f, 48.0f, 72.0f};
        for (float x : mainHallWindowXs)
        {
            DrawMainHallWindow(x, 2.55f);
            DrawMainHallWindow(x, 9.0f);
        }

        // Three evenly spaced gates on each extended wing, aligned to the side stands.
        constexpr float wingDoorZs[] = {-72.0f, -104.0f, -136.0f};
        for (float z : wingDoorZs)
        {
            DrawSideDoor(LeftInnerX + 0.16f, z);
            DrawSideDoor(RightInnerX - 0.16f, z);
        }

        // Ground-floor windows sit between gates; the extended upper row
        // continues toward the wing ends without adding second-storey doors.
        constexpr float wingGroundWindowZs[] = {-40.0f, -88.0f, -120.0f};
        for (float z : wingGroundWindowZs)
        {
            DrawSideWindow(LeftInnerX + 0.13f, z, 2.55f, 8.0f, 3.2f);
            DrawSideWindow(RightInnerX - 0.13f, z, 2.55f, 8.0f, 3.2f);
        }
        constexpr float wingUpperWindowZs[] = {-24.0f, -56.0f, -88.0f, -120.0f, -136.0f};
        for (float z : wingUpperWindowZs)
        {
            DrawSideWindow(LeftInnerX + 0.13f, z, 9.0f, 7.0f, 3.2f);
            DrawSideWindow(RightInnerX - 0.13f, z, 9.0f, 7.0f, 3.2f);
        }
    }

    void DrawStand(float centerX, float centerZ, float halfX, float halfZ)
    {
        constexpr Color sideLine{218, 222, 211, 255};
        constexpr Color centerLine{220, 174, 82, 255};
        // These shallow 3D paint bars have world-space width, unlike GL line
        // primitives, which remain one pixel wide and can flicker in perspective.
        constexpr float paintY = 0.0275f;
        constexpr float paintThickness = 0.025f;
        constexpr float paintWidth = 0.36f;
        constexpr float endInset = 0.75f;
        constexpr float dashGap = 1.5f;
        centerX *= TerminalScale;
        centerZ *= TerminalScale;
        centerZ += TerminalZOffset;
        halfX *= TerminalScale;
        halfZ *= TerminalScale;

        const auto drawDashedLineZ = [&](float x, float lineLength)
        {
            const int dashCount = static_cast<int>((lineLength + 1.0f) / 6.0f);
            const float dashLength = (lineLength - dashGap * (dashCount - 1)) / dashCount;
            const float firstDashZ = centerZ - lineLength * 0.5f + dashLength * 0.5f;
            for (int dash = 0; dash < dashCount; ++dash)
            {
                const float z = firstDashZ + dash * (dashLength + dashGap);
                DrawCubeV({x, paintY, z}, {paintWidth, paintThickness, dashLength}, sideLine);
            }
        };

        const auto drawDashedLineX = [&](float z, float lineLength)
        {
            const int dashCount = static_cast<int>((lineLength + 1.0f) / 6.0f);
            const float dashLength = (lineLength - dashGap * (dashCount - 1)) / dashCount;
            const float firstDashX = centerX - lineLength * 0.5f + dashLength * 0.5f;
            for (int dash = 0; dash < dashCount; ++dash)
            {
                const float x = firstDashX + dash * (dashLength + dashGap);
                DrawCubeV({x, paintY, z}, {dashLength, paintThickness, paintWidth}, sideLine);
            }
        };

        // Mark each stand along its longer axis so the three lines follow the
        // aircraft's direction: into the apron on the main facade, outward on
        // the side wings. The short end margins keep paint inside each space.
        if (halfZ >= halfX)
        {
            const float lineLength = 2.0f * halfZ - 2.0f * endInset;
            const float leftX = centerX - halfX + paintWidth * 0.5f;
            const float rightX = centerX + halfX - paintWidth * 0.5f;
            drawDashedLineZ(leftX, lineLength);
            drawDashedLineZ(rightX, lineLength);
            DrawCubeV({centerX, paintY, centerZ}, {paintWidth, paintThickness, lineLength}, centerLine);
        }
        else
        {
            const float lineLength = 2.0f * halfX - 2.0f * endInset;
            const float farZ = centerZ - halfZ + paintWidth * 0.5f;
            const float nearZ = centerZ + halfZ - paintWidth * 0.5f;
            drawDashedLineX(farZ, lineLength);
            drawDashedLineX(nearZ, lineLength);
            DrawCubeV({centerX, paintY, centerZ}, {lineLength, paintThickness, paintWidth}, centerLine);
        }
    }

    void DrawApronAndStands()
    {
        // The apron and the service-area spur meet edge-to-edge to form one paved platform.
        constexpr float platformDepth = 430.0f;
        constexpr float platformCenterZ = -55.0f;
        constexpr float platformThickness = 1.0f;
        constexpr float serviceSpurDepth = 225.0f;
        constexpr float serviceSpurCenterZ = -67.5f;
        const Color platformColor{105, 121, 122, 255};
        DrawCubeV(
            {0.0f, -0.01f - platformThickness * 0.5f, platformCenterZ},
            {440.0f, platformThickness, platformDepth},
            platformColor);
        DrawCubeV(
            {250.0f, -0.01f - platformThickness * 0.5f, serviceSpurCenterZ},
            {60.0f, platformThickness, serviceSpurDepth},
            platformColor);

        // Four stands face the main section and three serve each wing (ten total).
        // The main row sits slightly closer to its boarding facade while leaving
        // a clear apron lane between the terminal canopy and stand markings.
        constexpr float backStandXs[] = {-27.0f, -9.0f, 9.0f, 27.0f};
        for (float x : backStandXs) DrawStand(x, -20.0f, 5.5f, 8.0f);
        constexpr float wingStandZs[] = {-72.0f, -104.0f, -136.0f};
        for (float z : wingStandZs)
        {
            DrawStand(-SideStandCenterX, z, 10.0f, 8.0f);
            DrawStand(SideStandCenterX, z, 10.0f, 8.0f);
        }
    }
}

void AirportTerminal::Draw() const
{
    DrawApronAndStands();
    DrawTerminal();
}

const std::vector<AirportParkingStand>& AirportTerminal::GetParkingStands() const
{
    static const std::vector<AirportParkingStand> stands = []
    {
        std::vector<AirportParkingStand> result;
        constexpr float backStandXs[] = {-27.0f, -9.0f, 9.0f, 27.0f};
        for (int i = 0; i < 4; ++i)
            result.push_back({"GATE_" + std::to_string(i + 1),
                {backStandXs[i] * TerminalScale, 0.035f,
                    -20.0f * TerminalScale + TerminalZOffset}, 180.0f,
                AircraftSize::Small, 1});

        // These coordinates match the painted side stands and their gate-view
        // hit targets. Keeping route nodes on the markings makes parked planes
        // stop in the actual stand rather than on the taxi lane.
        constexpr float wingStandZs[] = {-72.0f, -104.0f, -136.0f};
        for (int row = 0; row < 3; ++row)
        {
            result.push_back({"WEST_" + std::to_string(row + 1),
                {-SideStandCenterX * TerminalScale, 0.035f,
                    wingStandZs[row] * TerminalScale + TerminalZOffset}, -90.0f,
                AircraftSize::Large, 1});
            result.push_back({"EAST_" + std::to_string(row + 1),
                {SideStandCenterX * TerminalScale, 0.035f,
                    wingStandZs[row] * TerminalScale + TerminalZOffset}, 90.0f,
                AircraftSize::Large, 1});
        }
        return result;
    }();
    return stands;
}

const AirportParkingStand* AirportTerminal::GetAdjacentGate(
    const std::string& gateId, int direction) const
{
    constexpr const char* navigationOrder[] = {
        "GATE_1", "GATE_2", "GATE_3", "GATE_4",
        "EAST_1", "EAST_2", "EAST_3",
        "WEST_3", "WEST_2", "WEST_1"
    };
    constexpr int GateCount = static_cast<int>(
        sizeof(navigationOrder) / sizeof(navigationOrder[0]));

    int currentIndex = -1;
    for (int i = 0; i < GateCount; ++i)
    {
        if (gateId == navigationOrder[i])
        {
            currentIndex = i;
            break;
        }
    }
    if (currentIndex < 0) return nullptr;

    const int step = direction < 0 ? -1 : 1;
    const int adjacentIndex = (currentIndex + step + GateCount) % GateCount;
    for (const AirportParkingStand& stand : GetParkingStands())
    {
        if (stand.id == navigationOrder[adjacentIndex]) return &stand;
    }
    return nullptr;
}

bool AirportTerminal::GetGateMarkingPosition(
    const std::string& gateId, Vector3& position) const
{
    for (const AirportParkingStand& stand : GetParkingStands())
    {
        if (stand.id == gateId)
        {
            position = stand.position;
            if (gateId.rfind("WEST_", 0) == 0 || gateId.rfind("EAST_", 0) == 0)
            {
                const std::size_t separator = gateId.find('_');
                const int row = std::atoi(gateId.c_str() + separator + 1) - 1;
                constexpr float markingWingZs[] = {-72.0f, -104.0f, -136.0f};
                if (row < 0 || row >= static_cast<int>(sizeof(markingWingZs) / sizeof(markingWingZs[0])))
                    return false;
                position.x = (gateId.rfind("WEST_", 0) == 0 ? -SideStandCenterX : SideStandCenterX) *
                    TerminalScale;
                position.z = markingWingZs[row] * TerminalScale + TerminalZOffset;
            }
            return true;
        }
    }
    return false;
}

const AirportParkingStand* AirportTerminal::GetGateAtScreenPosition(
    Vector2 screenPosition, const Camera3D& camera) const
{
    const Ray ray = GetScreenRay(screenPosition, camera);
    if (std::fabs(ray.direction.y) < 0.0001f) return nullptr;

    constexpr float GroundY = 0.06f;
    constexpr float ClickPadding = 2.5f;
    const float distance = (GroundY - ray.position.y) / ray.direction.y;
    if (distance < 0.0f) return nullptr;
    const Vector3 point{
        ray.position.x + ray.direction.x * distance,
        GroundY,
        ray.position.z + ray.direction.z * distance
    };

    for (const AirportParkingStand& stand : GetParkingStands())
    {
        Vector3 markingPosition{};
        if (!GetGateMarkingPosition(stand.id, markingPosition)) continue;

        const bool topGate = stand.id.rfind("GATE_", 0) == 0;
        const float halfWidth = (topGate ? 5.5f : 10.0f) * TerminalScale + ClickPadding;
        const float halfDepth = 8.0f * TerminalScale + ClickPadding;
        if (std::fabs(point.x - markingPosition.x) <= halfWidth &&
            std::fabs(point.z - markingPosition.z) <= halfDepth)
            return &stand;
    }
    return nullptr;
}

void AirportTerminal::DrawGateHighlight(const std::string& gateId) const
{
    Vector3 position{};
    if (!GetGateMarkingPosition(gateId, position)) return;

    const bool topGate = gateId.rfind("GATE_", 0) == 0;
    const float width = (topGate ? 11.0f : 20.0f) * TerminalScale;
    const float depth = 16.0f * TerminalScale;
    constexpr float Y = 0.07f;
    constexpr float Border = 0.32f;
    const Color fill{48, 205, 198, 72};
    const Color outline{79, 239, 222, 245};

    DrawCubeV({position.x, Y, position.z},
        {width, 0.035f, depth}, fill);
    DrawCubeV({position.x, Y + 0.03f, position.z - depth * 0.5f},
        {width, 0.06f, Border}, outline);
    DrawCubeV({position.x, Y + 0.03f, position.z + depth * 0.5f},
        {width, 0.06f, Border}, outline);
    DrawCubeV({position.x - width * 0.5f, Y + 0.03f, position.z},
        {Border, 0.06f, depth}, outline);
    DrawCubeV({position.x + width * 0.5f, Y + 0.03f, position.z},
        {Border, 0.06f, depth}, outline);
}
