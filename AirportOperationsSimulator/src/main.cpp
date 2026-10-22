#include "raylib.h"

#include "airport_terminal.h"
#include "airport_ground.h"
#include "airplane.h"
#include "airport_fire_truck.h"
#include "airport_service_vehicles.h"
#include "follow_me_car.h"
#include "baggage_tug.h"
#include "camera_controller.h"
#include "car_parking_lot.h"
#include "passenger_shuttle.h"
#include "service_building.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    constexpr float GateNavigationButtonWidth = 144.0f;
    constexpr float GateNavigationButtonHeight = 32.0f;
    constexpr float GateNavigationButtonGap = 8.0f;
    constexpr float GateNavigationButtonMargin = 16.0f;
    constexpr float ServiceCrewButtonWidth = 200.0f;

    struct GateNavigationButtons
    {
        Rectangle previous;
        Rectangle next;
        Rectangle overview;
        Rectangle bringServiceCrew;
    };

    GateNavigationButtons GetGateNavigationButtons()
    {
        const float buttonY = static_cast<float>(
            GetScreenHeight() - static_cast<int>(GateNavigationButtonHeight) - 16);
        return {
            {GateNavigationButtonMargin, buttonY,
                GateNavigationButtonWidth, GateNavigationButtonHeight},
            {GateNavigationButtonMargin + GateNavigationButtonWidth + GateNavigationButtonGap,
                buttonY, GateNavigationButtonWidth, GateNavigationButtonHeight},
            {static_cast<float>(GetScreenWidth()) - GateNavigationButtonMargin -
                GateNavigationButtonWidth, buttonY,
                GateNavigationButtonWidth, GateNavigationButtonHeight},
            {(static_cast<float>(GetScreenWidth()) - ServiceCrewButtonWidth) * 0.5f,
                buttonY - 96.0f, ServiceCrewButtonWidth, GateNavigationButtonHeight}
        };
    }

    void DrawGateNavigationButton(Rectangle bounds, const char* label)
    {
        DrawRectangleRec(bounds, Color{25, 64, 78, 245});
        DrawRectangleLinesEx(bounds, 2.0f, Color{112, 211, 213, 255});
        constexpr int FontSize = 13;
        const int textWidth = MeasureText(label, FontSize);
        const int textX = static_cast<int>(bounds.x + (bounds.width - textWidth) * 0.5f);
        const int textY = static_cast<int>(
            bounds.y + (bounds.height - FontSize) * 0.5f);
        DrawText(label, textX, textY, FontSize, RAYWHITE);
    }

    struct FlightPlan
    {
        Airplane airplane;
        std::string flightName;
        std::string airline;
        std::string origin;
        std::string destination;
        Color livery{48, 125, 160, 255};
        float arrivalScheduledSeconds{-1.0f};
        float departureScheduledSeconds{-1.0f};
        float preparationTime{0.0f};
        bool hasArrivalSchedule{false};
        bool spawned{false};
        bool standAssigned{false};
        bool departureFlight{false};
        bool arrivalFlight{false};
        bool routeStarted{false};
        bool departureQueued{false};
        bool taxiToStandQueued{false};
        bool runwayReserved{false};
        bool takeoffRollStarted{false};
        std::uint64_t requestOrder{0};
        std::vector<std::string> activeRouteNodeIds;
        std::string arrivalStartNode;
        std::string landingExitNodeId;
        std::string parkingStandId;
        std::string departureHoldNodeId;
        std::string runwayId{"RUNWAY_A"};
        std::string departureRunwayStart{"RUNWAY_START_W"};
        float runwayZ{0.0f};
        float parkingHeadingDegrees{0.0f};
    };

    std::vector<Vector2> MakeWorldRoute(const AirportGround& ground,
        const std::vector<std::string>& nodeIds)
    {
        std::vector<Vector2> route;
        route.reserve(nodeIds.size());
        for (const std::string& id : nodeIds)
        {
            const TaxiwayNode* node = ground.FindNode(id);
            if (node == nullptr) return {};
            route.push_back({node->position.x, node->position.z});
        }
        return route;
    }

    std::vector<Vector2> MakeTakeoffRoute(const AirportGround& ground,
        const std::string& runwayId, const std::string& startNodeId, Vector2 currentPosition)
    {
        const auto runway = std::find_if(ground.GetRunways().begin(), ground.GetRunways().end(),
            [&runwayId](const AirportRunway& item) { return item.id == runwayId; });
        if (runway == ground.GetRunways().end()) return {};
        const bool westStart = startNodeId.find("START_W") != std::string::npos;
        const float direction = westStart ? 1.0f : -1.0f;
        const float farRunwayEnd = runway->center.x + direction * runway->length * 0.5f;
        const float rolloutEnd = farRunwayEnd + direction * 50.0f;
        const float startX = currentPosition.x;
        const float runwayZ = runway->center.z;
        std::vector<Vector2> route;
        route.reserve(6);
        route.push_back(currentPosition);
        for (float fraction : {0.2f, 0.42f, 0.66f, 0.86f, 1.0f})
            route.push_back({startX + (rolloutEnd - startX) * fraction, runwayZ});
        return route;
    }

    int FindRunwayExitWaypoint(const AirportGround& ground,
        const std::vector<std::string>& route, float runwayZ)
    {
        bool leftRunwayCenterline = false;
        for (std::size_t i = 1; i < route.size(); ++i)
        {
            const TaxiwayNode* node = ground.FindNode(route[i]);
            if (node == nullptr) continue;
            if (std::abs(node->position.z - runwayZ) > 1.0f)
                leftRunwayCenterline = true;
            if (!leftRunwayCenterline) continue;

            const std::string& id = route[i];
            const bool runwayExitConnector = id.find("UPPER_EXIT") != std::string::npos ||
                id.find("LOWER_EXIT") != std::string::npos ||
                id.rfind("RWY_EXIT_", 0) == 0 || id.find("_TURN_") != std::string::npos;
            if (!runwayExitConnector && std::abs(node->position.z - runwayZ) > 20.0f)
                return static_cast<int>(i);
        }
        return -1;
    }

    std::string FormatShiftTime(float shiftSeconds)
    {
        const int totalMinutes = 6 * 60 + static_cast<int>(shiftSeconds / 60.0f);
        char buffer[8]{};
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d",
            (totalMinutes / 60) % 24, totalMinutes % 60);
        return buffer;
    }

    const char* FlightStateLabel(AirplaneState state)
    {
        switch (state)
        {
        case AirplaneState::Holding: return "HOLD";
        case AirplaneState::Arriving: return "INBOUND";
        case AirplaneState::Landing: return "LANDING";
        case AirplaneState::AwaitingTaxiAuthorization: return "AWAIT TAXI";
        case AirplaneState::TaxiingToStand: return "TAXI IN";
        case AirplaneState::Parked: return "PARKED";
        case AirplaneState::PreparingForDeparture: return "PREP";
        case AirplaneState::TaxiingToRunway: return "TAXI OUT";
        case AirplaneState::AwaitingTakeoffAuthorization: return "HOLD SHORT";
        case AirplaneState::TakingOff: return "TAKEOFF";
        case AirplaneState::Departed: return "DONE";
        case AirplaneState::Delayed: return "DELAYED";
        case AirplaneState::Incident: return "INCIDENT";
        default: return "SCHEDULED";
        }
    }

    Vector2 ProjectAirportPoint(Vector2 point, const Camera3D& camera)
    {
        return GetWorldToScreen({point.x, 1.1f, point.y}, camera);
    }

    void DrawStateIndicator(AirplaneState state, Vector2 center, float size)
    {
        const float half = size * 0.5f;
        switch (state)
        {
        case AirplaneState::Holding:
            DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), half,
                Color{247, 184, 68, 255});
            DrawRectangle(static_cast<int>(center.x - half * 0.35f),
                static_cast<int>(center.y - half * 0.45f), static_cast<int>(size * 0.18f),
                static_cast<int>(size * 0.9f), Color{247, 184, 68, 255});
            DrawRectangle(static_cast<int>(center.x + half * 0.17f),
                static_cast<int>(center.y - half * 0.45f), static_cast<int>(size * 0.18f),
                static_cast<int>(size * 0.9f), Color{247, 184, 68, 255});
            break;
        case AirplaneState::AwaitingTaxiAuthorization:
        case AirplaneState::AwaitingTakeoffAuthorization:
            DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), half,
                Color{79, 194, 228, 255});
            DrawLineEx({center.x - half * 0.45f, center.y},
                {center.x + half * 0.25f, center.y}, 2.0f, Color{79, 194, 228, 255});
            DrawTriangle({center.x + half * 0.55f, center.y},
                {center.x + half * 0.05f, center.y - half * 0.35f},
                {center.x + half * 0.05f, center.y + half * 0.35f},
                Color{79, 194, 228, 255});
            break;
        case AirplaneState::Arriving:
            DrawTriangle({center.x, center.y + half}, {center.x - half, center.y - half},
                {center.x + half, center.y - half}, Color{77, 190, 225, 255});
            break;
        case AirplaneState::Landing:
            DrawLineEx({center.x - half, center.y - half * 0.35f}, center, 3.0f,
                Color{77, 190, 225, 255});
            DrawLineEx({center.x + half, center.y - half * 0.35f}, center, 3.0f,
                Color{77, 190, 225, 255});
            DrawLineEx(center, {center.x, center.y + half}, 3.0f, Color{77, 190, 225, 255});
            break;
        case AirplaneState::TaxiingToStand:
        case AirplaneState::TaxiingToRunway:
            DrawRectangle(static_cast<int>(center.x - half), static_cast<int>(center.y - 2.0f),
                static_cast<int>(size * 0.6f), 4, Color{250, 213, 84, 255});
            DrawTriangle({center.x + half, center.y}, {center.x + half * 0.1f, center.y - half},
                {center.x + half * 0.1f, center.y + half}, Color{250, 213, 84, 255});
            break;
        case AirplaneState::Parked:
            DrawRectangle(static_cast<int>(center.x - half), static_cast<int>(center.y - half),
                static_cast<int>(size), static_cast<int>(size), Color{74, 200, 137, 255});
            DrawCircleV(center, size * 0.16f, Color{20, 55, 46, 255});
            break;
        case AirplaneState::PreparingForDeparture:
            DrawCircleV(center, half * 0.72f, Color{247, 184, 68, 255});
            for (int i = 0; i < 4; ++i)
            {
                const float angle = i * 1.5707963f;
                DrawLineEx({center.x + std::cos(angle) * half * 0.82f,
                    center.y + std::sin(angle) * half * 0.82f},
                    {center.x + std::cos(angle) * half * 1.25f,
                    center.y + std::sin(angle) * half * 1.25f}, 2.0f,
                    Color{247, 184, 68, 255});
            }
            break;
        case AirplaneState::TakingOff:
            DrawTriangle({center.x, center.y - half}, {center.x - half, center.y + half},
                {center.x + half, center.y + half}, Color{74, 200, 137, 255});
            break;
        case AirplaneState::Delayed:
            DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), half,
                Color{247, 144, 63, 255});
            DrawLineEx({center.x, center.y - half * 0.55f}, {center.x, center.y + half * 0.15f},
                3.0f, Color{247, 144, 63, 255});
            DrawCircleV({center.x, center.y + half * 0.55f}, 1.8f, Color{247, 144, 63, 255});
            break;
        case AirplaneState::Incident:
            DrawTriangle({center.x, center.y - half}, {center.x - half, center.y + half},
                {center.x + half, center.y + half}, Color{231, 76, 76, 255});
            DrawLineEx({center.x, center.y - half * 0.35f}, {center.x, center.y + half * 0.25f},
                2.0f, RAYWHITE);
            DrawCircleV({center.x, center.y + half * 0.55f}, 1.6f, RAYWHITE);
            break;
        default:
            DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), half,
                Color{150, 169, 177, 255});
            break;
        }
    }

    void DrawAirplanePreview3D(const Airplane& airplane, Rectangle bounds,
        const Camera3D& previewCamera, RenderTexture2D& previewTarget)
    {
        BeginTextureMode(previewTarget);
        ClearBackground(Color{27, 42, 51, 255});
        BeginMode3D(previewCamera);
        airplane.Draw({0.0f, 0.0f}, 0.0f,
            airplane.size == AircraftSize::Large ? 0.78f : 0.62f);
        EndMode3D();
        EndTextureMode();

        const Rectangle source{0.0f, 0.0f,
            static_cast<float>(previewTarget.texture.width),
            -static_cast<float>(previewTarget.texture.height)};
        DrawTexturePro(previewTarget.texture, source, bounds, {0.0f, 0.0f}, 0.0f, WHITE);

        // Screen-space registration label sits over the fuselage in the preview.
        const int tagWidth = MeasureText(airplane.flightId.c_str(), 12) + 10;
        const int tagX = static_cast<int>(bounds.x + bounds.width * 0.5f - tagWidth * 0.5f);
        const int tagY = static_cast<int>(bounds.y + bounds.height * 0.55f - 8.0f);
        DrawRectangle(tagX, tagY, tagWidth, 16, Fade(Color{11, 25, 33, 255}, 0.9f));
        DrawText(airplane.flightId.c_str(), tagX + 5, tagY + 2, 12, RAYWHITE);
    }

    void DrawMenuRunwayDigit(char digit, float x, float z)
    {
        const bool segments[10][7] = {
            {true, true, true, true, true, true, false},    // 0
            {true, true, false, true, true, false, true},   // 2
            {true, true, true, true, false, false, true},   // 9
            {true, true, true, false, false, false, false} // 7
        };
        int row = -1;
        if (digit == '0') row = 0;
        else if (digit == '2') row = 1;
        else if (digit == '9') row = 2;
        else if (digit == '7') row = 3;
        if (row < 0) return;

        constexpr float y = 0.112f;
        constexpr float barWidth = 3.3f;
        constexpr float barDepth = 0.72f;
        constexpr float halfWidth = 1.65f;
        constexpr float halfDepth = 2.45f;
        const Color paint{235, 236, 226, 255};
        const auto bar = [paint](float centerX, float centerZ, float sx, float sz)
        {
            DrawCubeV({centerX, y, centerZ}, {sx, 0.035f, sz}, paint);
        };
        if (segments[row][0]) bar(x, z - halfDepth, barWidth, barDepth);
        if (segments[row][1]) bar(x + halfWidth, z - 1.25f, barDepth, 2.45f);
        if (segments[row][2]) bar(x + halfWidth, z + 1.25f, barDepth, 2.45f);
        if (segments[row][3]) bar(x, z + halfDepth, barWidth, barDepth);
        if (segments[row][4]) bar(x - halfWidth, z + 1.25f, barDepth, 2.45f);
        if (segments[row][5]) bar(x - halfWidth, z - 1.25f, barDepth, 2.45f);
        if (segments[row][6]) bar(x, z, barWidth, barDepth);
    }

    void DrawMenuBackground(float time)
    {
        const int width = GetScreenWidth();
        const int height = GetScreenHeight();
        const float w = static_cast<float>(width);
        const float h = static_cast<float>(height);
        const float horizon = h * 0.60f;
        ClearBackground(Color{143, 184, 203, 255});
        DrawRectangleGradientV(0, 0, width, height,
            Color{115, 164, 188, 255}, Color{231, 190, 143, 255});

        // Thin cloud bands and a hazy horizon keep the sky spacious and calm.
        for (int i = 0; i < 5; ++i)
        {
            const float x = w * (0.48f + 0.13f * static_cast<float>(i));
            const float y = h * (0.16f + 0.075f * static_cast<float>(i % 3));
            const float cloudWidth = w * (0.09f + 0.018f * static_cast<float>(i % 2));
            DrawEllipse(static_cast<int>(x), static_cast<int>(y), cloudWidth, h * 0.008f,
                Fade(Color{231, 242, 240, 255}, 0.14f));
            DrawEllipse(static_cast<int>(x + cloudWidth * 0.45f), static_cast<int>(y + h * 0.012f),
                cloudWidth * 0.52f, h * 0.006f, Fade(Color{244, 230, 209, 255}, 0.12f));
        }

        // Keep the familiar ground color and simple tree line behind the 3D airfield.
        DrawRectangle(0, static_cast<int>(horizon), width, static_cast<int>(h - horizon),
            Color{84, 116, 103, 255});
        for (int i = 0; i < 34; ++i)
        {
            const float x = w * static_cast<float>(i) / 33.0f;
            const float treeTop = horizon + h * (0.016f + 0.012f *
                (0.5f + 0.5f * std::sin(static_cast<float>(i) * 1.7f)));
            DrawTriangle({x - w * 0.014f, horizon + h * 0.065f},
                {x, treeTop}, {x + w * 0.014f, horizon + h * 0.065f},
                Color{57, 91, 82, 255});
        }

        // Build a small, true 3D airfield vignette rather than drawing a flat map.
        const Camera3D menuCamera{
            {170.0f, 45.0f, 260.0f}, {45.0f, 55.0f, -70.0f},
            {0.0f, 1.0f, 0.0f}, 38.0f, CAMERA_PERSPECTIVE};
        BeginMode3D(menuCamera);

        constexpr float runwayX = 86.0f;
        constexpr float runwayNearZ = 155.0f;
        constexpr float runwayFarZ = -175.0f;
        constexpr float runwayLength = runwayNearZ - runwayFarZ;
        DrawCubeV({runwayX, -0.08f, (runwayNearZ + runwayFarZ) * 0.5f},
            {51.0f, 0.18f, runwayLength + 10.0f}, Color{79, 86, 89, 255});
        DrawCubeV({runwayX, 0.015f, (runwayNearZ + runwayFarZ) * 0.5f},
            {42.0f, 0.16f, runwayLength}, Color{48, 55, 60, 255});

        const Color runwayPaint{235, 236, 226, 255};
        for (float edgeX : {runwayX - 19.0f, runwayX + 19.0f})
            DrawCubeV({edgeX, 0.105f, -10.0f}, {0.62f, 0.025f, runwayLength - 22.0f}, runwayPaint);
        for (int dash = 0; dash < 9; ++dash)
        {
            const float z = runwayFarZ + 38.0f + static_cast<float>(dash) * 31.0f;
            DrawCubeV({runwayX, 0.11f, z}, {0.8f, 0.03f, 14.0f}, runwayPaint);
        }
        for (float thresholdZ : {runwayFarZ + 12.0f, runwayNearZ - 12.0f})
            for (int stripe = 0; stripe < 8; ++stripe)
            {
                const float x = runwayX - 17.5f + static_cast<float>(stripe) * 5.0f;
                DrawCubeV({x, 0.11f, thresholdZ}, {2.0f, 0.03f, 2.4f}, runwayPaint);
            }
        for (float endSign : {-1.0f, 1.0f})
        {
            const float z = endSign < 0.0f ? runwayFarZ + 27.0f : runwayNearZ - 27.0f;
            for (float side : {-1.0f, 1.0f})
                DrawCubeV({runwayX + side * 8.0f, 0.11f,
                    z + (endSign < 0.0f ? 5.0f : -5.0f)}, {6.0f, 0.03f, 13.0f}, runwayPaint);
            const char first = endSign < 0.0f ? '0' : '2';
            const char second = endSign < 0.0f ? '9' : '7';
            DrawMenuRunwayDigit(first, runwayX, z - 4.3f);
            DrawMenuRunwayDigit(second, runwayX, z + 4.3f);
        }

        // Two menu buildings echo the in-game terminal and service building.
        DrawCubeV({runwayX, 0.015f, -199.0f}, {100.0f, 0.12f, 34.0f}, Color{130, 137, 129, 255});
        const Color terminalWall{196, 185, 156, 255};
        const Color terminalRoof{37, 66, 81, 255};
        const Color terminalGlass{47, 119, 143, 255};
        const Color gateOrange{196, 112, 65, 255};

        // Broad two-storey concourse with two long wings forming an open apron.
        DrawCubeV({runwayX, 8.0f, -231.0f}, {100.0f, 16.0f, 18.0f}, terminalWall);
        DrawCubeV({runwayX, 16.7f, -231.0f}, {106.0f, 2.2f, 24.0f}, terminalRoof);
        for (float wingSide : {-1.0f, 1.0f})
        {
            const float wingX = runwayX + wingSide * 43.0f;
            DrawCubeV({wingX, 8.0f, -204.0f}, {14.0f, 16.0f, 48.0f}, terminalWall);
            DrawCubeV({wingX, 16.7f, -204.0f}, {19.0f, 2.2f, 53.0f}, terminalRoof);
            const float courtyardFaceX = wingX - wingSide * 7.15f;
            for (int bay = 0; bay < 5; ++bay)
            {
                const float z = -222.0f + static_cast<float>(bay) * 8.5f;
                DrawCubeV({courtyardFaceX, 10.5f, z}, {0.25f, 4.5f, 4.8f}, terminalGlass);
                if (bay % 2 == 0)
                {
                    DrawCubeV({courtyardFaceX, 3.5f, z}, {0.45f, 5.4f, 4.5f}, gateOrange);
                    DrawCubeV({courtyardFaceX, 6.5f, z}, {3.0f, 0.7f, 6.2f}, Color{89, 119, 126, 255});
                }
            }
        }
        // Repeating glazing and gate portals across the terminal's apron-facing hall.
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 13; ++col)
            {
                const float x = runwayX - 39.0f + static_cast<float>(col) * 6.5f;
                const float y = row == 0 ? 5.0f : 11.5f;
                const Color glass = col % 5 == 0 ? Color{211, 180, 122, 255} : terminalGlass;
                DrawCubeV({x, y, -221.75f}, {3.6f, 3.0f, 0.3f}, glass);
                if (row == 0 && col % 3 == 1)
                {
                    DrawCubeV({x, 3.2f, -221.65f}, {3.5f, 5.2f, 0.45f}, gateOrange);
                    DrawCubeV({x, 6.1f, -219.5f}, {5.2f, 0.65f, 4.0f}, Color{89, 119, 126, 255});
                }
            }

        // Offset upper storey and stepped roof give the second building the same
        // compact, practical service-block silhouette as the in-game model.
        const float serviceX = runwayX + 78.0f;
        const float serviceZ = -219.0f;
        DrawCubeV({serviceX, 0.9f, serviceZ}, {39.0f, 1.8f, 32.0f}, Color{113, 124, 119, 255});
        DrawCubeV({serviceX, 7.5f, serviceZ}, {36.0f, 13.2f, 29.0f}, Color{169, 177, 161, 255});
        DrawCubeV({serviceX - 4.0f, 17.0f, serviceZ - 2.0f}, {27.0f, 7.0f, 22.0f}, Color{196, 185, 156, 255});
        DrawCubeV({serviceX, 14.5f, serviceZ}, {39.0f, 1.0f, 32.0f}, Color{43, 72, 84, 255});
        DrawCubeV({serviceX - 4.0f, 21.0f, serviceZ - 2.0f}, {31.0f, 1.0f, 26.0f}, terminalRoof);
        for (int win = 0; win < 5; ++win)
        {
            const float x = serviceX - 14.0f + static_cast<float>(win) * 7.0f;
            DrawCubeV({x, 7.3f, serviceZ + 14.65f}, {3.8f, 3.4f, 0.3f}, terminalGlass);
            if (win < 4)
                DrawCubeV({x - 3.0f, 17.2f, serviceZ + 9.2f}, {3.6f, 2.8f, 0.3f},
                    win == 1 ? Color{211, 180, 122, 255} : terminalGlass);
        }

        // Use the same detailed 3D aircraft renderer as gameplay. Each nose follows
        // its own route direction, including the descending traffic stream.
        struct MenuFlight
        {
            Vector2 from;
            Vector2 to;
            float period;
            float phase;
            float startAltitude;
            float finishAltitude;
            float scale;
            Color livery;
        };
        const MenuFlight menuFlights[] = {
            {{-25.0f, 80.0f}, {185.0f, -170.0f}, 21.0f, 0.02f, 42.0f, 56.0f, 0.25f, Color{47, 165, 193, 255}},
            {{185.0f, -165.0f}, {-25.0f, 120.0f}, 24.0f, 0.44f, 58.0f, 28.0f, 0.23f, Color{232, 154, 69, 255}},
            {{175.0f, 78.0f}, {10.0f, -155.0f}, 27.0f, 0.68f, 48.0f, 40.0f, 0.19f, Color{223, 229, 221, 255}},
            {{-10.0f, -125.0f}, {125.0f, -205.0f}, 30.0f, 0.31f, 36.0f, 42.0f, 0.17f, Color{119, 186, 146, 255}}
        };
        static Airplane menuAircraft[4];
        static bool menuAircraftInitialized = false;
        if (!menuAircraftInitialized)
        {
            for (int i = 0; i < 4; ++i)
                menuAircraft[i].liveryColor = menuFlights[i].livery;
            menuAircraftInitialized = true;
        }
        for (int i = 0; i < 4; ++i)
        {
            const MenuFlight& flight = menuFlights[i];
            const float progress = std::fmod(time / flight.period + flight.phase, 1.0f);
            Airplane& aircraft = menuAircraft[i];
            aircraft.position = {
                flight.from.x + (flight.to.x - flight.from.x) * progress,
                flight.from.y + (flight.to.y - flight.from.y) * progress};
            aircraft.headingDegrees = std::atan2(flight.to.x - flight.from.x,
                -(flight.to.y - flight.from.y)) * RAD2DEG;
            aircraft.altitude = flight.startAltitude +
                (flight.finishAltitude - flight.startAltitude) * progress;
            aircraft.pitchDegrees = i == 1 ? -2.0f : 0.0f;
            aircraft.speed = 5.0f;
            aircraft.Draw(flight.scale);
        }
        EndMode3D();
    }
}

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1024, 768, "Airport Operations Simulator");

    CameraController cameraController;
    const Camera3D airplanePreviewCamera{
        {38.0f, 98.0f, -120.0f}, {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, 35.0f, CAMERA_PERSPECTIVE};
    RenderTexture2D airplanePreviewTarget = LoadRenderTexture(256, 192);
    AirportTerminal terminal;
    const auto makeStaticAircraftAtGate = [&](const std::string& gateId,
        AircraftSize size, Color livery)
    {
        Airplane aircraft;
        aircraft.flightId = "STATIC_" + gateId;
        aircraft.aircraftType = size == AircraftSize::Large ?
            "Passenger jet" : "Regional jet";
        aircraft.size = size;
        aircraft.liveryColor = livery;
        aircraft.state = AirplaneState::Parked;
        aircraft.renderScale = size == AircraftSize::Large ? 0.21f : 0.17f;
        for (const AirportParkingStand& stand : terminal.GetParkingStands())
        {
            if (stand.id == gateId)
            {
                aircraft.position = {stand.position.x, stand.position.z};
                aircraft.headingDegrees = stand.headingDegrees;
                break;
            }
        }
        return aircraft;
    };
    Airplane staticGate1Aircraft = makeStaticAircraftAtGate(
        "GATE_1", AircraftSize::Small, Color{38, 145, 189, 255});
    Airplane staticEastWingAircraft = makeStaticAircraftAtGate(
        "EAST_1", AircraftSize::Large, Color{98, 174, 128, 255});
    AirportGround airportGround(terminal);
    bool showMainMenu = true;
    bool exitRequested = false;
    float menuAnimationTime = 0.0f;
    std::vector<FlightPlan> flights;
    // Keep the original parking bays available for later vehicle-movement work.
    constexpr Vector2 BaggageTugParkingPositions[] = {
        {230.85f, -112.25f},
        {225.75f, -112.25f},
        {220.65f, -112.25f},
        {215.55f, -112.25f},
        {210.45f, -112.25f}
    };
    constexpr Vector2 PassengerShuttleParkingPositions[] = {
        {164.55f, -112.25f},
        {169.65f, -112.25f},
        {174.75f, -112.25f}
    };
    constexpr Vector2 FireTruckParkingPositions[] = {
        {179.85f, -112.25f},
        {184.95f, -112.25f}
    };
    constexpr Vector2 RefuellingTankerParkingPositions[] = {
        {190.05f, -112.25f},
        {195.15f, -112.25f}
    };
    constexpr Vector2 FollowMeCarParkingPosition{190.05f, -39.75f};
    constexpr Vector2 PushbackTugParkingPositions[] = {
        {200.25f, -39.75f}, {205.35f, -39.75f}, {210.45f, -39.75f}
    };
    constexpr Vector2 BaggageBeltLoaderParkingPositions[] = {
        {164.55f, -39.75f}, {169.65f, -39.75f}, {174.75f, -39.75f},
        {179.85f, -39.75f}, {184.95f, -39.75f}
    };
    constexpr Vector2 CateringTruckParkingPositions[] = {
        {164.55f, -70.25f}, {169.65f, -70.25f}
    };
    constexpr Vector2 WaterServiceTruckParkingPositions[] = {
        {174.75f, -70.25f}, {179.85f, -70.25f}
    };
    constexpr Vector2 MaintenancePickupParkingPositions[] = {
        {184.95f, -70.25f}, {190.05f, -70.25f}
    };
    constexpr Vector2 BoardingStairsParkingPositions[] = {
        {195.15f, -70.25f}, {200.25f, -70.25f}, {205.35f, -70.25f},
        {210.45f, -70.25f}, {215.55f, -70.25f}, {220.65f, -70.25f},
        {225.75f, -70.25f}, {230.85f, -70.25f}
    };

    // These temporary gate-side crews are populated only when requested from the UI.
    std::vector<BaggageTug> baggageTugs;
    std::vector<AircraftRefuellingTanker> refuellingTankers;
    FollowMeCar followMeCars[] = {{FollowMeCarParkingPosition.x, FollowMeCarParkingPosition.y}};
    std::vector<AircraftPushbackTug> pushbackTugs;
    std::vector<BaggageBeltLoader> baggageBeltLoaders;
    std::vector<CateringTruck> cateringTrucks;
    std::vector<WaterServiceTruck> waterServiceTrucks;
    std::vector<MaintenancePickupTruck> maintenancePickupTrucks;
    std::vector<PassengerBoardingStairs> passengerBoardingStairs;

    std::vector<BaggageTug> parkedBaggageTugs{
        {BaggageTugParkingPositions[0].x, BaggageTugParkingPositions[0].y},
        {BaggageTugParkingPositions[1].x, BaggageTugParkingPositions[1].y},
        {BaggageTugParkingPositions[2].x, BaggageTugParkingPositions[2].y},
        {BaggageTugParkingPositions[3].x, BaggageTugParkingPositions[3].y},
        {BaggageTugParkingPositions[4].x, BaggageTugParkingPositions[4].y}
    };
    PassengerShuttle passengerShuttles[] = {
        {PassengerShuttleParkingPositions[0].x, PassengerShuttleParkingPositions[0].y},
        {PassengerShuttleParkingPositions[1].x, PassengerShuttleParkingPositions[1].y},
        {PassengerShuttleParkingPositions[2].x, PassengerShuttleParkingPositions[2].y}
    };
    AirportFireTruck airportFireTrucks[] = {
        {FireTruckParkingPositions[0].x, FireTruckParkingPositions[0].y},
        {FireTruckParkingPositions[1].x, FireTruckParkingPositions[1].y}
    };
    std::vector<AircraftRefuellingTanker> parkedRefuellingTankers{
        {RefuellingTankerParkingPositions[0].x, RefuellingTankerParkingPositions[0].y},
        {RefuellingTankerParkingPositions[1].x, RefuellingTankerParkingPositions[1].y}
    };
    std::vector<AircraftPushbackTug> parkedPushbackTugs{
        {PushbackTugParkingPositions[0].x, PushbackTugParkingPositions[0].y},
        {PushbackTugParkingPositions[1].x, PushbackTugParkingPositions[1].y},
        {PushbackTugParkingPositions[2].x, PushbackTugParkingPositions[2].y}
    };
    std::vector<BaggageBeltLoader> parkedBaggageBeltLoaders{
        {BaggageBeltLoaderParkingPositions[0].x, BaggageBeltLoaderParkingPositions[0].y},
        {BaggageBeltLoaderParkingPositions[1].x, BaggageBeltLoaderParkingPositions[1].y},
        {BaggageBeltLoaderParkingPositions[2].x, BaggageBeltLoaderParkingPositions[2].y},
        {BaggageBeltLoaderParkingPositions[3].x, BaggageBeltLoaderParkingPositions[3].y},
        {BaggageBeltLoaderParkingPositions[4].x, BaggageBeltLoaderParkingPositions[4].y}
    };
    std::vector<CateringTruck> parkedCateringTrucks{
        {CateringTruckParkingPositions[0].x, CateringTruckParkingPositions[0].y},
        {CateringTruckParkingPositions[1].x, CateringTruckParkingPositions[1].y}
    };
    std::vector<WaterServiceTruck> parkedWaterServiceTrucks{
        {WaterServiceTruckParkingPositions[0].x, WaterServiceTruckParkingPositions[0].y},
        {WaterServiceTruckParkingPositions[1].x, WaterServiceTruckParkingPositions[1].y}
    };
    std::vector<MaintenancePickupTruck> parkedMaintenancePickupTrucks{
        {MaintenancePickupParkingPositions[0].x, MaintenancePickupParkingPositions[0].y},
        {MaintenancePickupParkingPositions[1].x, MaintenancePickupParkingPositions[1].y}
    };
    std::vector<PassengerBoardingStairs> parkedPassengerBoardingStairs{
        {BoardingStairsParkingPositions[0].x, BoardingStairsParkingPositions[0].y},
        {BoardingStairsParkingPositions[1].x, BoardingStairsParkingPositions[1].y},
        {BoardingStairsParkingPositions[2].x, BoardingStairsParkingPositions[2].y},
        {BoardingStairsParkingPositions[3].x, BoardingStairsParkingPositions[3].y},
        {BoardingStairsParkingPositions[4].x, BoardingStairsParkingPositions[4].y},
        {BoardingStairsParkingPositions[5].x, BoardingStairsParkingPositions[5].y},
        {BoardingStairsParkingPositions[6].x, BoardingStairsParkingPositions[6].y},
        {BoardingStairsParkingPositions[7].x, BoardingStairsParkingPositions[7].y}
    };
    ServiceBuilding serviceBuilding;
    CarParkingLot carParkingLot;
    // Inbound flights enter on separate runways and use the existing graph to
    // reach different stands. Route nodes also include the curved taxiway
    // samples, so the aircraft can follow the authored bends smoothly.
    struct ArrivalSetup { const char* flight; const char* name; const char* airline;
        const char* origin; const char* destination; Color livery; const char* type; const char* approach;
        AircraftSize size; const char* runwayId; const char* runwayStart; const char* stand; float runwayZ;
        float arrivalMinute; float departureMinute; };
    const ArrivalSetup arrivals[] = {
        {"AO100", "Aurora", "Northstar Air", "Reykjavik (KEF)", "Dublin (DUB)", {38, 145, 189, 255}, "Passenger jet", "APPROACH_A_W", AircraftSize::Large, "RUNWAY_A", "RUNWAY_START_W", "GATE_1", -507.0f, 0.0f, 12.0f},
        {"AO202", "Meridian 202", "Meridian Air", "Copenhagen (CPH)", "Oslo (OSL)", {219, 124, 58, 255}, "Regional jet", "APPROACH_B_W", AircraftSize::Small, "RUNWAY_B", "RUNWAY_B_START_W", "WEST_2", -642.0f, 4.0f, 17.0f},
        {"AO317", "Freightliner 317", "CargoLink", "Frankfurt (FRA)", "Prague (PRG)", {211, 185, 76, 255}, "Cargo jet", "APPROACH_C_W", AircraftSize::Large, "RUNWAY_C", "RUNWAY_C_START_W", "EAST_3", -777.0f, 8.0f, 22.0f},
        {"AO512", "Skyline 512", "Skyline Airways", "Bristol (BRS)", "Amsterdam (AMS)", {98, 174, 128, 255}, "Passenger jet", "APPROACH_A_W", AircraftSize::Large, "RUNWAY_A", "RUNWAY_START_W", "GATE_2", -507.0f, 15.0f, 29.0f}
    };
    for (const ArrivalSetup& setup : arrivals)
    {
        FlightPlan plan;
        plan.airplane.flightId = setup.flight;
        plan.airplane.aircraftType = setup.type;
        plan.airplane.size = setup.size;
        plan.flightName = setup.name;
        plan.airline = setup.airline;
        plan.origin = setup.origin;
        plan.destination = setup.destination;
        plan.livery = setup.livery;
        plan.airplane.liveryColor = setup.livery;
        const TaxiwayNode* approach = airportGround.FindNode(setup.approach);
        if (approach == nullptr) continue;
        plan.airplane.position = {approach->position.x, approach->position.z};
        plan.airplane.headingDegrees = 90.0f;
        plan.airplane.renderScale = setup.size == AircraftSize::Large ? 0.21f : 0.17f;
        plan.airplane.isPriority = std::string(setup.flight) == "AO202";
        plan.hasArrivalSchedule = true;
        plan.arrivalScheduledSeconds = setup.arrivalMinute * 60.0f;
        plan.departureScheduledSeconds = setup.departureMinute * 60.0f;
        plan.arrivalStartNode = setup.approach;
        plan.parkingStandId = setup.stand;
        plan.runwayId = setup.runwayId;
        plan.departureRunwayStart = setup.runwayStart;
        plan.departureHoldNodeId = std::string(setup.runwayId) == "RUNWAY_A" ?
            "A_EXIT_W" : (std::string(setup.runwayId) == "RUNWAY_B" ?
                "AB_EXIT_W" : "BC_EXIT_W");
        plan.runwayZ = setup.runwayZ;
        plan.departureFlight = true;
        for (const AirportParkingStand& stand : terminal.GetParkingStands())
        {
            if (stand.id == plan.parkingStandId)
            {
                plan.parkingHeadingDegrees = stand.headingDegrees;
                break;
            }
        }
        plan.arrivalFlight = false;
        flights.push_back(std::move(plan));
    }

    // A fourth aircraft demonstrates the basic parked -> preparation -> taxi
    // -> takeoff sequence without adding traffic-control rules.
    if (const TaxiwayNode* stand = airportGround.FindNode("GATE_4"))
    {
        FlightPlan plan;
        plan.airplane.flightId = "AO440";
        plan.airplane.aircraftType = "Narrow-body jet";
        plan.airplane.size = AircraftSize::Small;
        plan.flightName = "Coastal 440";
        plan.airline = "Coastal Air";
        plan.origin = "Airport (HOME)";
        plan.destination = "Oslo (OSL)";
        plan.livery = {87, 158, 192, 255};
        plan.airplane.liveryColor = plan.livery;
        plan.airplane.position = {stand->position.x, stand->position.z};
        plan.airplane.headingDegrees = 180.0f;
        plan.airplane.state = AirplaneState::Parked;
        plan.airplane.renderScale = 0.17f;
        for (const AirportParkingStand& parkingStand : terminal.GetParkingStands())
        {
            if (parkingStand.id == "GATE_4")
            {
                plan.parkingHeadingDegrees = parkingStand.headingDegrees;
                break;
            }
        }
        plan.departureFlight = true;
        plan.departureScheduledSeconds = 5.0f * 60.0f;
        plan.parkingStandId = "GATE_4";
        plan.runwayId = "RUNWAY_A";
        plan.departureHoldNodeId = "A_EXIT_W";
        flights.push_back(std::move(plan));
    }

    std::deque<std::size_t> arrivalQueue;
    std::deque<std::size_t> taxiToStandQueue;
    std::deque<std::size_t> departureQueue;
    std::unordered_map<std::string, std::string> occupiedNodes;
    std::unordered_map<std::string, std::string> occupiedSections;
    std::unordered_map<std::string, std::string> occupiedStands;
    occupiedNodes["GATE_1"] = staticGate1Aircraft.flightId;
    occupiedStands["GATE_1"] = staticGate1Aircraft.flightId;
    occupiedNodes["EAST_1"] = staticEastWingAircraft.flightId;
    occupiedStands["EAST_1"] = staticEastWingAircraft.flightId;
    std::unordered_map<std::string, std::string> runwayOwners;
    std::uint64_t nextRequestOrder = 0;
    float shiftClockSeconds = 0.0f;
    int selectedFlightIndex = -1;
    std::string selectedGateId;
    std::string serviceCrewGateId;
    std::string refuellingMessage;
    float refuellingMessageTime = 0.0f;
    bool gate1RefuellingStarted = false;
    bool eastWingRefuellingStarted = false;
    bool gate1RefuellingFinished = false;
    bool eastWingRefuellingFinished = false;
    float gate1RefuellingTimeRemaining = 0.0f;
    float eastWingRefuellingTimeRemaining = 0.0f;
    enum class RefuellingTankerMotion
    {
        Parked,
        ApproachingRear,
        ApproachingAircraft,
        Refuelling,
        ReturningBehindAircraft,
        ReturningToCrewPosition
    };
    RefuellingTankerMotion refuellingTankerMotion = RefuellingTankerMotion::Parked;
    std::string activeRefuellingGate;
    Vector2 refuellingTankerCrewPosition{};
    Vector2 refuellingTankerRearStagingPosition{};
    Vector2 refuellingTankerAircraftPosition{};
    constexpr float ShiftClockRate = 15.0f; // 15 simulated seconds per real second.
    const auto sectionKey = [](const std::string& first, const std::string& second)
    {
        return first < second ? first + "|" + second : second + "|" + first;
    };
    const auto routeCanBeReserved = [&](const std::vector<std::string>& route,
        const std::string& flightId)
    {
        if (route.empty()) return false;
        for (const std::string& nodeId : route)
        {
            const auto owner = occupiedNodes.find(nodeId);
            if (owner != occupiedNodes.end() && owner->second != flightId) return false;
        }
        for (std::size_t i = 1; i < route.size(); ++i)
        {
            const auto owner = occupiedSections.find(sectionKey(route[i - 1], route[i]));
            if (owner != occupiedSections.end() && owner->second != flightId) return false;
        }
        return true;
    };
    const auto reserveRoute = [&](const std::vector<std::string>& route,
        const std::string& flightId)
    {
        for (const std::string& nodeId : route) occupiedNodes[nodeId] = flightId;
        for (std::size_t i = 1; i < route.size(); ++i)
            occupiedSections[sectionKey(route[i - 1], route[i])] = flightId;
    };
    const auto releaseRoute = [&](const std::vector<std::string>& route,
        const std::string& flightId)
    {
        for (const std::string& nodeId : route)
        {
            const auto owner = occupiedNodes.find(nodeId);
            if (owner != occupiedNodes.end() && owner->second == flightId)
                occupiedNodes.erase(owner);
        }
        for (std::size_t i = 1; i < route.size(); ++i)
        {
            const std::string key = sectionKey(route[i - 1], route[i]);
            const auto owner = occupiedSections.find(key);
            if (owner != occupiedSections.end() && owner->second == flightId)
                occupiedSections.erase(owner);
        }
    };
    const auto selectGate = [&](const AirportParkingStand& gate)
    {
        selectedGateId = gate.id;
        selectedFlightIndex = -1;
        Vector3 focus = gate.position;
        terminal.GetGateMarkingPosition(selectedGateId, focus);
        cameraController.EnterGateView(focus, gate.headingDegrees);
    };
    const auto selectAdjacentGate = [&](int direction)
    {
        if (const AirportParkingStand* gate =
            terminal.GetAdjacentGate(selectedGateId, direction))
            selectGate(*gate);
    };
    const auto bringServiceCrewToSelectedGate = [&]()
    {
        if (refuellingTankerMotion != RefuellingTankerMotion::Parked ||
            (gate1RefuellingStarted && !gate1RefuellingFinished) ||
            (eastWingRefuellingStarted && !eastWingRefuellingFinished))
        {
            refuellingMessage = "The tanker must stay here until refuelling finishes.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (selectedGateId.empty()) return;

        const AirportParkingStand* selectedGate = nullptr;
        const AirportParkingStand* referenceGate = nullptr;
        for (const AirportParkingStand& stand : terminal.GetParkingStands())
        {
            if (stand.id == selectedGateId) selectedGate = &stand;
            if (stand.id == "GATE_4") referenceGate = &stand;
        }
        if (selectedGate == nullptr || referenceGate == nullptr) return;

        if (baggageTugs.empty())
        {
            if (parkedBaggageTugs.empty() || parkedRefuellingTankers.empty() ||
                parkedPushbackTugs.empty() || parkedBaggageBeltLoaders.empty() ||
                parkedCateringTrucks.empty() || parkedWaterServiceTrucks.empty() ||
                parkedMaintenancePickupTrucks.empty() ||
                parkedPassengerBoardingStairs.size() < 2)
                return;

            baggageTugs.push_back(std::move(parkedBaggageTugs.front()));
            parkedBaggageTugs.erase(parkedBaggageTugs.begin());
            refuellingTankers.push_back(std::move(parkedRefuellingTankers.front()));
            parkedRefuellingTankers.erase(parkedRefuellingTankers.begin());
            pushbackTugs.push_back(std::move(parkedPushbackTugs.front()));
            parkedPushbackTugs.erase(parkedPushbackTugs.begin());
            baggageBeltLoaders.push_back(std::move(parkedBaggageBeltLoaders.front()));
            parkedBaggageBeltLoaders.erase(parkedBaggageBeltLoaders.begin());
            cateringTrucks.push_back(std::move(parkedCateringTrucks.front()));
            parkedCateringTrucks.erase(parkedCateringTrucks.begin());
            waterServiceTrucks.push_back(std::move(parkedWaterServiceTrucks.front()));
            parkedWaterServiceTrucks.erase(parkedWaterServiceTrucks.begin());
            maintenancePickupTrucks.push_back(std::move(parkedMaintenancePickupTrucks.front()));
            parkedMaintenancePickupTrucks.erase(parkedMaintenancePickupTrucks.begin());
            for (int stair = 0; stair < 2; ++stair)
            {
                passengerBoardingStairs.push_back(
                    std::move(parkedPassengerBoardingStairs.front()));
                parkedPassengerBoardingStairs.erase(
                    parkedPassengerBoardingStairs.begin());
            }
        }

        // Preserve the existing temporary Gate 4 service layout as offsets from
        // its stand, then rotate those placements to match the selected gate.
        constexpr Vector2 placements[] = {
            {-15.5f, -20.0f}, // baggage tug
            {15.5f, -10.0f},  // refuelling tanker
            {0.5f, -30.0f},   // pushback tug
            {15.5f, 4.0f},    // belt loader
            {-15.5f, 4.0f},   // catering truck
            {15.5f, -25.0f},  // water service truck
            {-15.5f, -9.0f},  // maintenance pickup
            {-10.0f, 0.0f},   // boarding stairs
            {10.5f, 0.0f}     // boarding stairs
        };
        const bool isWingGate = selectedGateId.rfind("EAST_", 0) == 0 ||
            selectedGateId.rfind("WEST_", 0) == 0;
        constexpr float WingVehicleSpacingScale = 1.3f;
        constexpr float WingStairsSpacingScale = 1.5f;
        const float angle = (selectedGate->headingDegrees -
            referenceGate->headingDegrees) * DEG2RAD;
        // Raylib's Y rotation is opposite to the aircraft's local-coordinate
        // transform, so invert the heading delta for the vehicle models.
        const float vehicleHeadingDegrees = referenceGate->headingDegrees -
            selectedGate->headingDegrees;
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        const auto place = [&](Vector2 offset, float wingSpacingScale)
        {
            if (isWingGate)
            {
                offset.x *= wingSpacingScale;
                offset.y *= wingSpacingScale;
            }
            // Match Airplane::DrawPose's positive-heading rotation convention.
            // The old inverse rotation mirrored the layout on the wing stands.
            const float x = offset.x * cosine - offset.y * sine;
            const float z = offset.x * sine + offset.y * cosine;
            return Vector2{selectedGate->position.x + x,
                selectedGate->position.z + z};
        };
        const Vector2 tug = place(placements[0], WingVehicleSpacingScale);
        baggageTugs.front().SetPosition(tug.x, tug.y);
        baggageTugs.back().SetHeadingDegrees(vehicleHeadingDegrees);
        const Vector2 tanker = place(placements[1], WingVehicleSpacingScale);
        refuellingTankers.front().SetPosition(tanker.x, tanker.y);
        refuellingTankers.back().SetHeadingDegrees(vehicleHeadingDegrees);
        refuellingTankerCrewPosition = tanker;
        const Vector2 pushback = place(placements[2], WingVehicleSpacingScale);
        pushbackTugs.front().SetPosition(pushback.x, pushback.y);
        pushbackTugs.back().SetHeadingDegrees(vehicleHeadingDegrees);
        const Vector2 beltLoader = place(placements[3], WingVehicleSpacingScale);
        baggageBeltLoaders.front().SetPosition(beltLoader.x, beltLoader.y);
        baggageBeltLoaders.back().SetHeadingDegrees(vehicleHeadingDegrees);
        const Vector2 catering = place(placements[4], WingVehicleSpacingScale);
        cateringTrucks.front().SetPosition(catering.x, catering.y);
        cateringTrucks.back().SetHeadingDegrees(vehicleHeadingDegrees);
        const Vector2 water = place(placements[5], WingVehicleSpacingScale);
        waterServiceTrucks.front().SetPosition(water.x, water.y);
        waterServiceTrucks.back().SetHeadingDegrees(vehicleHeadingDegrees);
        const Vector2 maintenance = place(placements[6], WingVehicleSpacingScale);
        maintenancePickupTrucks.front().SetPosition(maintenance.x, maintenance.y);
        maintenancePickupTrucks.back().SetHeadingDegrees(vehicleHeadingDegrees);
        for (std::size_t i = 7; i < 9; ++i)
        {
            const Vector2 stairs = place(placements[i], WingStairsSpacingScale);
            const std::size_t stairIndex = i - 7;
            passengerBoardingStairs[stairIndex].SetPosition(stairs.x, stairs.y);
            passengerBoardingStairs[stairIndex].SetHeadingDegrees(vehicleHeadingDegrees);
        }
        serviceCrewGateId = selectedGateId;
    };
    const auto refuellingIndicatorBounds = [&](const Airplane& aircraft)
    {
        const Vector2 screen = ProjectAirportPoint(aircraft.position,
            cameraController.GetCamera3D());
        return Rectangle{screen.x - 26.0f, screen.y - 70.0f, 52.0f, 58.0f};
    };
    const auto getRefuellingApproachWaypoints = [&](const Airplane& aircraft,
        const AircraftRefuellingTanker& tanker)
    {
        const Vector2 tankerPosition = tanker.GetPosition();
        const float radians = aircraft.headingDegrees * DEG2RAD;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        const float dx = tankerPosition.x - aircraft.position.x;
        const float dz = tankerPosition.y - aircraft.position.y;
        const float localSide = dx * cosine + dz * sine;
        const float localLongitudinal = -dx * sine + dz * cosine;
        const float side = localSide >= 0.0f ? 1.0f : -1.0f;
        const float stagingX = side * (44.0f * aircraft.renderScale + 4.5f);
        const float rearZ = std::max(localLongitudinal,
            38.0f * aircraft.renderScale + 6.0f);
        const auto toWorld = [&](float localX, float localZ)
        {
            return Vector2{
                aircraft.position.x + localX * cosine - localZ * sine,
                aircraft.position.y + localX * sine + localZ * cosine
            };
        };
        return std::pair<Vector2, Vector2>{
            toWorld(stagingX, rearZ), toWorld(0.0f, rearZ)};
    };
    const auto handleRefuellingClick = [&](const std::string& gateId,
        bool& refuellingStarted, bool& refuellingFinished,
        float& refuellingTimeRemaining, const Airplane& aircraft)
    {
        if (serviceCrewGateId != gateId || refuellingTankers.empty())
        {
            refuellingMessage = "Bring the fuel tanker to this gate first.";
        }
        else if (refuellingStarted || refuellingFinished ||
            refuellingTankerMotion != RefuellingTankerMotion::Parked)
        {
            refuellingMessage = "Refuelling has already started.";
        }
        else
        {
            const auto waypoints = getRefuellingApproachWaypoints(
                aircraft, refuellingTankers.front());
            refuellingTankerRearStagingPosition = waypoints.first;
            refuellingTankerAircraftPosition = waypoints.second;
            activeRefuellingGate = gateId;
            refuellingTankerMotion = RefuellingTankerMotion::ApproachingRear;
            refuellingMessage = "Fuel tanker approaching from behind.";
        }
        refuellingMessageTime = 2.5f;
    };
    const auto updateRefuelling = [&](float& timeRemaining, bool& finished,
        const char* aircraftName, float deltaTime)
    {
        if (timeRemaining <= 0.0f || finished) return;
        timeRemaining = std::max(0.0f, timeRemaining - deltaTime);
        if (timeRemaining == 0.0f)
        {
            finished = true;
            refuellingMessage = std::string(aircraftName) + " refuelling finished.";
            refuellingMessageTime = 2.5f;
        }
    };
    const auto moveTankerToward = [&](Vector2 target, float deltaTime)
    {
        if (refuellingTankers.empty()) return false;
        constexpr float TankerApproachSpeed = 9.0f;
        const Vector2 position = refuellingTankers.front().GetPosition();
        const float dx = target.x - position.x;
        const float dz = target.y - position.y;
        const float distance = std::sqrt(dx * dx + dz * dz);
        const float step = TankerApproachSpeed * deltaTime;
        if (distance <= step || distance <= 0.05f)
        {
            refuellingTankers.front().SetPosition(target.x, target.y);
            return true;
        }
        refuellingTankers.front().SetPosition(
            position.x + dx / distance * step,
            position.y + dz / distance * step);
        return false;
    };
    const auto updateTankerMotion = [&](float deltaTime)
    {
        switch (refuellingTankerMotion)
        {
        case RefuellingTankerMotion::ApproachingRear:
            if (moveTankerToward(refuellingTankerRearStagingPosition, deltaTime))
                refuellingTankerMotion = RefuellingTankerMotion::ApproachingAircraft;
            break;
        case RefuellingTankerMotion::ApproachingAircraft:
            if (moveTankerToward(refuellingTankerAircraftPosition, deltaTime))
            {
                refuellingTankerMotion = RefuellingTankerMotion::Refuelling;
                if (activeRefuellingGate == "GATE_1")
                {
                    gate1RefuellingStarted = true;
                    gate1RefuellingTimeRemaining = 6.0f;
                }
                else if (activeRefuellingGate == "EAST_1")
                {
                    eastWingRefuellingStarted = true;
                    eastWingRefuellingTimeRemaining = 6.0f;
                }
                refuellingMessage = "Refuelling started.";
                refuellingMessageTime = 2.5f;
            }
            break;
        case RefuellingTankerMotion::ReturningBehindAircraft:
            if (moveTankerToward(refuellingTankerRearStagingPosition, deltaTime))
                refuellingTankerMotion = RefuellingTankerMotion::ReturningToCrewPosition;
            break;
        case RefuellingTankerMotion::ReturningToCrewPosition:
            if (moveTankerToward(refuellingTankerCrewPosition, deltaTime))
            {
                refuellingTankerMotion = RefuellingTankerMotion::Parked;
                activeRefuellingGate.clear();
            }
            break;
        case RefuellingTankerMotion::Parked:
        case RefuellingTankerMotion::Refuelling:
            break;
        }
    };

    while (!WindowShouldClose())
    {
        const float deltaTime = GetFrameTime();
        if (exitRequested) break;
        if (showMainMenu)
            menuAnimationTime += deltaTime;
        else
        {
            shiftClockSeconds += deltaTime * ShiftClockRate;
            refuellingMessageTime = std::max(0.0f, refuellingMessageTime - deltaTime);
            updateRefuelling(gate1RefuellingTimeRemaining, gate1RefuellingFinished,
                "Gate 1", deltaTime);
            updateRefuelling(eastWingRefuellingTimeRemaining, eastWingRefuellingFinished,
                "East Wing Gate 1", deltaTime);
            if (refuellingTankerMotion == RefuellingTankerMotion::Refuelling &&
                ((activeRefuellingGate == "GATE_1" && gate1RefuellingFinished) ||
                    (activeRefuellingGate == "EAST_1" && eastWingRefuellingFinished)))
                refuellingTankerMotion = RefuellingTankerMotion::ReturningBehindAircraft;
            updateTankerMotion(deltaTime);
            cameraController.Update(deltaTime);
        }
        if (showMainMenu && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            const float menuX = static_cast<float>(GetScreenWidth()) * 0.085f;
            const float menuY = static_cast<float>(GetScreenHeight()) * 0.355f;
            const Rectangle startButton{menuX, menuY, 330.0f, 58.0f};
            const Rectangle settingsButton{menuX, menuY + 72.0f, 330.0f, 58.0f};
            const Rectangle exitButton{menuX, menuY + 144.0f, 330.0f, 58.0f};
            const Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, startButton)) showMainMenu = false;
            else if (CheckCollisionPointRec(mouse, exitButton)) exitRequested = true;
            else if (CheckCollisionPointRec(mouse, settingsButton)) { /* Settings placeholder. */ }
        }
        else if (showMainMenu && IsKeyPressed(KEY_ENTER)) showMainMenu = false;
        else if (showMainMenu && IsKeyPressed(KEY_ESCAPE)) exitRequested = true;
        else if (!showMainMenu && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            const Vector2 mouse = GetMousePosition();
            if (!gate1RefuellingFinished && CheckCollisionPointRec(mouse,
                refuellingIndicatorBounds(staticGate1Aircraft)))
                handleRefuellingClick("GATE_1", gate1RefuellingStarted,
                    gate1RefuellingFinished, gate1RefuellingTimeRemaining,
                    staticGate1Aircraft);
            else if (!eastWingRefuellingFinished && CheckCollisionPointRec(mouse,
                refuellingIndicatorBounds(staticEastWingAircraft)))
                handleRefuellingClick("EAST_1", eastWingRefuellingStarted,
                    eastWingRefuellingFinished, eastWingRefuellingTimeRemaining,
                    staticEastWingAircraft);
            else if (cameraController.IsGateViewActive())
            {
                const GateNavigationButtons buttons = GetGateNavigationButtons();
                if (CheckCollisionPointRec(mouse, buttons.previous))
                    selectAdjacentGate(-1);
                else if (CheckCollisionPointRec(mouse, buttons.next))
                    selectAdjacentGate(1);
                else if (CheckCollisionPointRec(mouse, buttons.bringServiceCrew))
                    bringServiceCrewToSelectedGate();
                else if (CheckCollisionPointRec(mouse, buttons.overview))
                {
                    cameraController.ReturnToOverview();
                    selectedGateId.clear();
                }
            }
            else
            {
                const bool clickedSchedulePanel =
                    mouse.x <= 406.0f && mouse.y >= 74.0f && mouse.y <= 214.0f;
                const int rightPanelLeft = GetScreenWidth() - 326;
                const bool clickedInfoPanel = selectedFlightIndex >= 0 &&
                    mouse.x >= rightPanelLeft && mouse.y >= 70.0f && mouse.y <= 398.0f;
                if (clickedInfoPanel)
                {
                    const Rectangle taxiButton{static_cast<float>(rightPanelLeft + 14), 324.0f, 284.0f, 24.0f};
                    const Rectangle priorityButton{static_cast<float>(rightPanelLeft + 14), 352.0f, 284.0f, 28.0f};
                    FlightPlan& selected = flights[static_cast<std::size_t>(selectedFlightIndex)];
                    if (CheckCollisionPointRec(mouse, priorityButton))
                        selected.airplane.isPriority = !selected.airplane.isPriority;
                    else if (CheckCollisionPointRec(mouse, taxiButton))
                    {
                        if (selected.airplane.state == AirplaneState::AwaitingTaxiAuthorization &&
                            !selected.taxiToStandQueued)
                        {
                            selected.taxiToStandQueued = true;
                            selected.requestOrder = nextRequestOrder++;
                            taxiToStandQueue.push_back(static_cast<std::size_t>(selectedFlightIndex));
                        }
                        else if (selected.airplane.state == AirplaneState::Parked && selected.departureFlight &&
                            !selected.departureQueued && selected.airplane.TransitionTo(AirplaneState::PreparingForDeparture))
                            selected.preparationTime = 3.0f;
                        else if (selected.airplane.state == AirplaneState::AwaitingTakeoffAuthorization)
                        {
                            const auto runway = std::find_if(airportGround.GetRunways().begin(),
                                airportGround.GetRunways().end(), [&selected](const AirportRunway& item)
                                { return item.id == selected.runwayId; });
                            const auto route = airportGround.FindRouteNodeIds(
                                selected.departureHoldNodeId, selected.departureRunwayStart);
                            if (runway != airportGround.GetRunways().end() && runway->available &&
                                runwayOwners.find(selected.runwayId) == runwayOwners.end() &&
                                routeCanBeReserved(route, selected.airplane.flightId))
                            {
                                std::vector<Vector2> takeoffRoute = MakeWorldRoute(airportGround, route);
                                if (!takeoffRoute.empty())
                                {
                                    const auto roll = MakeTakeoffRoute(airportGround, selected.runwayId,
                                        selected.departureRunwayStart, takeoffRoute.back());
                                    if (roll.size() > 1)
                                        takeoffRoute.insert(takeoffRoute.end(), roll.begin() + 1, roll.end());
                                    if (roll.size() > 1 && selected.airplane.TransitionTo(AirplaneState::TaxiingToRunway))
                                    {
                                        reserveRoute(route, selected.airplane.flightId);
                                        selected.activeRouteNodeIds = route;
                                        selected.routeStarted = true;
                                        selected.runwayReserved = true;
                                        selected.takeoffRollStarted = true;
                                        runwayOwners[selected.runwayId] = selected.airplane.flightId;
                                        occupiedNodes.erase(selected.departureHoldNodeId);
                                        selected.airplane.SetWaypointRoute(takeoffRoute, 72.0f,
                                            AirplaneState::Departed, static_cast<int>(route.size() - 1),
                                            AirplaneState::TakingOff);
                                    }
                                }
                            }
                        }
                    }
                }
                else if (!clickedSchedulePanel)
                {
                    // Resolve aircraft hits before the larger gate hit regions,
                    // since a parked aircraft can overlap its stand on screen.
                    int clickedFlight = -1;
                    float closestDistance = 34.0f;
                    for (std::size_t i = 0; i < flights.size(); ++i)
                    {
                        const FlightPlan& plan = flights[i];
                        if (!plan.spawned || plan.airplane.state == AirplaneState::Departed) continue;
                        const Vector2 projected = ProjectAirportPoint(plan.airplane.position,
                            cameraController.GetCamera3D());
                        const float dx = projected.x - mouse.x;
                        const float dy = projected.y - mouse.y;
                        const float distance = std::sqrt(dx * dx + dy * dy);
                        if (distance < closestDistance)
                        {
                            closestDistance = distance;
                            clickedFlight = static_cast<int>(i);
                        }
                    }
                    if (clickedFlight >= 0)
                    {
                        selectedGateId.clear();
                        selectedFlightIndex = clickedFlight;
                    }
                    else
                    {
                        const AirportParkingStand* clickedGate =
                            terminal.GetGateAtScreenPosition(mouse, cameraController.GetCamera3D());
                        if (clickedGate != nullptr)
                            selectGate(*clickedGate);
                        else
                        {
                            selectedGateId.clear();
                            selectedFlightIndex = -1;
                        }
                    }
                }
            }
        }
        if (!showMainMenu)
        {
        if (cameraController.IsGateViewActive())
        {
            if (IsKeyPressed(KEY_LEFT)) selectAdjacentGate(-1);
            else if (IsKeyPressed(KEY_RIGHT)) selectAdjacentGate(1);
            else if (IsKeyPressed(KEY_ESCAPE))
            {
                cameraController.ReturnToOverview();
                selectedGateId.clear();
            }
        }
        if (selectedFlightIndex >= 0 && selectedFlightIndex < static_cast<int>(flights.size()) &&
            IsKeyPressed(KEY_P))
            flights[static_cast<std::size_t>(selectedFlightIndex)].airplane.isPriority =
                !flights[static_cast<std::size_t>(selectedFlightIndex)].airplane.isPriority;
        for (std::size_t flightIndex = 0; flightIndex < flights.size(); ++flightIndex)
        {
            FlightPlan& plan = flights[flightIndex];
            Airplane& airplane = plan.airplane;
            if (!plan.spawned)
            {
                if (plan.hasArrivalSchedule && shiftClockSeconds >= plan.arrivalScheduledSeconds)
                {
                    const TaxiwayNode* approach = airportGround.FindNode(plan.arrivalStartNode);
                    if (approach == nullptr) continue;
                    // Start beyond the graph's runway approach point so the
                    // landing animation has a visible final descent in view.
                    airplane.position = {approach->position.x - 160.0f, approach->position.z};
                    airplane.altitude = 32.0f;
                    airplane.pitchDegrees = -5.0f;
                    airplane.headingDegrees = 90.0f;
                    airplane.TransitionTo(AirplaneState::Holding);
                    plan.arrivalFlight = true;
                    plan.spawned = true;
                    plan.requestOrder = nextRequestOrder++;
                    arrivalQueue.push_back(flightIndex);
                    occupiedNodes[plan.arrivalStartNode] = airplane.flightId;
                }
                else if (!plan.hasArrivalSchedule && plan.departureFlight &&
                    shiftClockSeconds >= plan.departureScheduledSeconds)
                {
                    const TaxiwayNode* stand = airportGround.FindNode(plan.parkingStandId);
                    if (stand == nullptr) continue;
                    airplane.position = {stand->position.x, stand->position.z};
                    airplane.state = AirplaneState::Parked;
                    plan.spawned = true;
                    plan.standAssigned = true;
                    occupiedStands[plan.parkingStandId] = airplane.flightId;
                    occupiedNodes[plan.parkingStandId] = airplane.flightId;
                }
            }
            if (!plan.spawned) continue;
            airplane.Update(deltaTime);
            if (airplane.state == AirplaneState::Parked)
                airplane.headingDegrees = plan.parkingHeadingDegrees;

            if (plan.arrivalFlight && plan.routeStarted &&
                airplane.state == AirplaneState::AwaitingTaxiAuthorization)
            {
                if (plan.runwayReserved)
                {
                    const auto owner = runwayOwners.find(plan.runwayId);
                    if (owner != runwayOwners.end() && owner->second == airplane.flightId)
                        runwayOwners.erase(owner);
                    plan.runwayReserved = false;
                }
                releaseRoute(plan.activeRouteNodeIds, airplane.flightId);
                plan.activeRouteNodeIds.clear();
                plan.routeStarted = false;
                if (!plan.landingExitNodeId.empty())
                    occupiedNodes[plan.landingExitNodeId] = airplane.flightId;
            }
            if (plan.arrivalFlight && plan.routeStarted && airplane.state == AirplaneState::Parked)
            {
                releaseRoute(plan.activeRouteNodeIds, airplane.flightId);
                plan.activeRouteNodeIds.clear();
                plan.routeStarted = false;
                plan.arrivalFlight = false;
                occupiedNodes[plan.parkingStandId] = airplane.flightId;
            }
            if (plan.departureFlight && plan.routeStarted &&
                airplane.state == AirplaneState::AwaitingTakeoffAuthorization)
            {
                releaseRoute(plan.activeRouteNodeIds, airplane.flightId);
                plan.activeRouteNodeIds.clear();
                plan.routeStarted = false;
                occupiedNodes[plan.departureHoldNodeId] = airplane.flightId;
            }
            if (plan.departureFlight && airplane.state == AirplaneState::Departed)
            {
                releaseRoute(plan.activeRouteNodeIds, airplane.flightId);
                plan.activeRouteNodeIds.clear();
                plan.routeStarted = false;
                const auto runwayOwner = runwayOwners.find(plan.runwayId);
                if (plan.runwayReserved && runwayOwner != runwayOwners.end() &&
                    runwayOwner->second == airplane.flightId)
                {
                    runwayOwners.erase(runwayOwner);
                    plan.runwayReserved = false;
                }
                const auto standOwner = occupiedStands.find(plan.parkingStandId);
                if (standOwner != occupiedStands.end() && standOwner->second == airplane.flightId)
                    occupiedStands.erase(standOwner);
                const auto standNodeOwner = occupiedNodes.find(plan.parkingStandId);
                if (standNodeOwner != occupiedNodes.end() &&
                    standNodeOwner->second == airplane.flightId)
                    occupiedNodes.erase(standNodeOwner);
                continue;
            }
            if (!plan.departureFlight || airplane.state == AirplaneState::Departed) continue;

            if (airplane.state == AirplaneState::PreparingForDeparture)
            {
                plan.preparationTime -= deltaTime;
                if (plan.preparationTime <= 0.0f)
                {
                    if (!plan.departureQueued)
                    {
                        plan.departureQueued = true;
                        plan.requestOrder = nextRequestOrder++;
                        departureQueue.push_back(static_cast<std::size_t>(&plan - flights.data()));
                    }
                }
            }
        }

        // Serve the oldest ready requests. Each runway has its own lease, so
        // independent runways can operate concurrently. Exclusive
        // reservations on route nodes and edges prevents runway and taxiway
        // conflicts. Blocked routes remain queued until a safe path is free.
        while (!arrivalQueue.empty() || !taxiToStandQueue.empty() || !departureQueue.empty())
        {
            enum class RequestType { Landing, TaxiToStand, Departure };
            struct Request
            {
                bool priority;
                std::uint64_t order;
                RequestType type;
                std::size_t flightIndex;
            };
            std::vector<Request> requests;
            requests.reserve(arrivalQueue.size() + taxiToStandQueue.size() + departureQueue.size());
            for (const std::size_t index : arrivalQueue)
                requests.push_back({flights[index].airplane.isPriority,
                    flights[index].requestOrder, RequestType::Landing, index});
            for (const std::size_t index : taxiToStandQueue)
                requests.push_back({flights[index].airplane.isPriority,
                    flights[index].requestOrder, RequestType::TaxiToStand, index});
            for (const std::size_t index : departureQueue)
                requests.push_back({flights[index].airplane.isPriority,
                    flights[index].requestOrder, RequestType::Departure, index});
            std::stable_sort(requests.begin(), requests.end(),
                [](const Request& left, const Request& right)
                {
                    if (left.priority != right.priority) return left.priority > right.priority;
                    return left.order < right.order;
                });

            bool foundSafeRequest = false;
            bool serveArrival = false;
            bool serveTaxiToStand = false;
            std::size_t flightIndex = 0;
            std::vector<std::string> routeNodeIds;
            for (const Request& request : requests)
            {
                FlightPlan& candidate = flights[request.flightIndex];
                const bool landingRequest = request.type == RequestType::Landing;
                const bool taxiRequest = request.type == RequestType::TaxiToStand;
                const std::string& start = landingRequest ? candidate.arrivalStartNode :
                    (taxiRequest ? candidate.landingExitNodeId : candidate.parkingStandId);
                const auto selectedRunway = std::find_if(airportGround.GetRunways().begin(),
                    airportGround.GetRunways().end(),
                    [&candidate](const AirportRunway& runway)
                    { return runway.id == candidate.runwayId; });
                if (landingRequest && (selectedRunway == airportGround.GetRunways().end() ||
                    !selectedRunway->available)) continue;
                if (landingRequest && runwayOwners.find(candidate.runwayId) != runwayOwners.end()) continue;
                std::vector<std::string> candidateRoute;
                std::string assignedStandId = candidate.parkingStandId;
                std::string assignedExitNodeId;
                float assignedStandHeading = candidate.parkingHeadingDegrees;
                if (landingRequest)
                {
                    std::vector<const AirportParkingStand*> standCandidates;
                    const auto addStandCandidates = [&](AircraftSize preferredSize,
                        bool preferredStandFirst)
                    {
                        if (preferredStandFirst)
                        {
                            for (const AirportParkingStand& stand : terminal.GetParkingStands())
                                if (stand.id == candidate.parkingStandId && stand.capacity > 0 &&
                                    stand.size == preferredSize &&
                                    (candidate.airplane.size == AircraftSize::Small ||
                                        stand.size == AircraftSize::Large))
                                    standCandidates.push_back(&stand);
                        }
                        for (const AirportParkingStand& stand : terminal.GetParkingStands())
                            if (stand.id != candidate.parkingStandId && stand.capacity > 0 &&
                                stand.size == preferredSize &&
                                (candidate.airplane.size == AircraftSize::Small ||
                                    stand.size == AircraftSize::Large))
                                standCandidates.push_back(&stand);
                    };
                    if (candidate.airplane.size == AircraftSize::Small)
                    {
                        addStandCandidates(AircraftSize::Small, true);
                        addStandCandidates(AircraftSize::Large, true);
                    }
                    else addStandCandidates(AircraftSize::Large, true);
                    bool foundStand = false;
                    for (const AirportParkingStand* stand : standCandidates)
                    {
                        const auto owner = occupiedStands.find(stand->id);
                        if (owner != occupiedStands.end() && owner->second != candidate.airplane.flightId)
                            continue;
                        std::vector<std::string> route = airportGround.FindRouteNodeIds(start, stand->id);
                        const int exitIndex = FindRunwayExitWaypoint(
                            airportGround, route, candidate.runwayZ);
                        if (route.empty() || exitIndex <= 0 ||
                            !routeCanBeReserved(std::vector<std::string>(route.begin(),
                                route.begin() + exitIndex + 1), candidate.airplane.flightId)) continue;
                        assignedExitNodeId = route[static_cast<std::size_t>(exitIndex)];
                        route.resize(static_cast<std::size_t>(exitIndex + 1));
                        candidateRoute = std::move(route);
                        assignedStandId = stand->id;
                        assignedStandHeading = stand->headingDegrees;
                        foundStand = true;
                        break;
                    }
                    if (!foundStand) continue;
                }
                else if (taxiRequest)
                {
                    if (candidate.landingExitNodeId.empty() || !candidate.standAssigned) continue;
                    candidateRoute = airportGround.FindRouteNodeIds(start, candidate.parkingStandId);
                    if (candidateRoute.empty() ||
                        !routeCanBeReserved(candidateRoute, candidate.airplane.flightId)) continue;
                }
                else
                {
                    candidateRoute = airportGround.FindRouteNodeIds(
                        start, candidate.departureHoldNodeId);
                    if (candidateRoute.empty() ||
                        !routeCanBeReserved(candidateRoute, candidate.airplane.flightId)) continue;
                }
                foundSafeRequest = true;
                serveArrival = landingRequest;
                serveTaxiToStand = taxiRequest;
                flightIndex = request.flightIndex;
                routeNodeIds = std::move(candidateRoute);
                if (landingRequest)
                {
                    candidate.parkingStandId = assignedStandId;
                    candidate.parkingHeadingDegrees = assignedStandHeading;
                    candidate.standAssigned = true;
                    candidate.landingExitNodeId = assignedExitNodeId;
                }
                break;
            }
            if (!foundSafeRequest) break;

            FlightPlan& plan = flights[flightIndex];
            auto& requestQueue = serveArrival ? arrivalQueue :
                (serveTaxiToStand ? taxiToStandQueue : departureQueue);
            const auto queuedRequest = std::find(requestQueue.begin(), requestQueue.end(), flightIndex);
            if (queuedRequest == requestQueue.end()) break;
            requestQueue.erase(queuedRequest);

            reserveRoute(routeNodeIds, plan.airplane.flightId);
            if (serveArrival) occupiedStands[plan.parkingStandId] = plan.airplane.flightId;
            plan.activeRouteNodeIds = routeNodeIds;
            plan.routeStarted = true;
            if (serveArrival)
            {
                plan.runwayReserved = true;
                runwayOwners[plan.runwayId] = plan.airplane.flightId;
            }
            if (serveArrival)
            {
                plan.airplane.TransitionTo(AirplaneState::Arriving);
                plan.airplane.TransitionTo(AirplaneState::Landing);
                const std::vector<Vector2> runwayRoute = MakeWorldRoute(airportGround, routeNodeIds);
                std::vector<Vector2> route;
                route.reserve(runwayRoute.size() + 4);
                route.push_back(plan.airplane.position);
                route.push_back({plan.airplane.position.x + 40.0f, plan.airplane.position.y});
                route.push_back({plan.airplane.position.x + 80.0f, plan.airplane.position.y});
                route.push_back({plan.airplane.position.x + 120.0f, plan.airplane.position.y});
                route.insert(route.end(), runwayRoute.begin(), runwayRoute.end());
                const int exitWaypoint = FindRunwayExitWaypoint(
                    airportGround, routeNodeIds, plan.runwayZ);
                // The runway threshold is graph node 1; after adding the
                // airborne lead-in, touchdown is at route point 5.
                route.resize(static_cast<std::size_t>(exitWaypoint + 5));
                plan.airplane.SetWaypointRoute(route, 32.0f,
                    AirplaneState::AwaitingTaxiAuthorization, -1,
                    AirplaneState::TaxiingToStand, 5);
            }
            else if (serveTaxiToStand)
            {
                plan.taxiToStandQueued = false;
                if (plan.airplane.TransitionTo(AirplaneState::TaxiingToStand))
                {
                    const std::vector<Vector2> route = MakeWorldRoute(airportGround, routeNodeIds);
                    plan.airplane.SetWaypointRoute(route, 9.0f, AirplaneState::Parked);
                }
            }
            else
            {
                plan.departureQueued = false;
                if (plan.airplane.TransitionTo(AirplaneState::TaxiingToRunway))
                {
                    const std::vector<Vector2> route = MakeWorldRoute(airportGround, routeNodeIds);
                    plan.airplane.SetWaypointRoute(route, 11.0f,
                        AirplaneState::AwaitingTakeoffAuthorization);
                }
            }
        }

        }

        BeginDrawing();
        ClearBackground(Color{142, 181, 199, 255});

        const Camera3D& sceneCamera = cameraController.GetCamera3D();
        BeginMode3D(sceneCamera);
        if (!showMainMenu)
        {
        terminal.Draw();
        // Draw taxiways after the terminal apron so the connected pavement and
        // centerlines remain visible where they cross the apron slab.
        airportGround.Draw();
        carParkingLot.Draw();
        if (!selectedGateId.empty())
            terminal.DrawGateHighlight(selectedGateId);
        serviceBuilding.Draw();
        for (const BaggageTug& baggageTug : parkedBaggageTugs)
            baggageTug.Draw();
        for (const PassengerShuttle& shuttle : passengerShuttles)
            shuttle.Draw();
        for (const AirportFireTruck& fireTruck : airportFireTrucks)
            fireTruck.Draw();
        for (const AircraftRefuellingTanker& refuellingTanker : parkedRefuellingTankers)
            refuellingTanker.Draw();
        for (const AircraftPushbackTug& pushbackTug : parkedPushbackTugs)
            pushbackTug.Draw();
        for (const BaggageBeltLoader& baggageBeltLoader : parkedBaggageBeltLoaders)
            baggageBeltLoader.Draw();
        for (const CateringTruck& cateringTruck : parkedCateringTrucks)
            cateringTruck.Draw();
        for (const WaterServiceTruck& waterServiceTruck : parkedWaterServiceTrucks)
            waterServiceTruck.Draw();
        for (const MaintenancePickupTruck& maintenancePickupTruck : parkedMaintenancePickupTrucks)
            maintenancePickupTruck.Draw();
        for (const PassengerBoardingStairs& boardingStairs : parkedPassengerBoardingStairs)
            boardingStairs.Draw();
        for (const BaggageTug& baggageTug : baggageTugs)
            baggageTug.Draw();
        for (const AircraftRefuellingTanker& refuellingTanker : refuellingTankers)
            refuellingTanker.Draw();
        for (const FollowMeCar& followMeCar : followMeCars)
            followMeCar.Draw();
        for (const AircraftPushbackTug& pushbackTug : pushbackTugs)
            pushbackTug.Draw();
        for (const BaggageBeltLoader& baggageBeltLoader : baggageBeltLoaders)
            baggageBeltLoader.Draw();
        for (const CateringTruck& cateringTruck : cateringTrucks)
            cateringTruck.Draw();
        for (const WaterServiceTruck& waterServiceTruck : waterServiceTrucks)
            waterServiceTruck.Draw();
        for (const MaintenancePickupTruck& maintenancePickupTruck : maintenancePickupTrucks)
            maintenancePickupTruck.Draw();
        for (const PassengerBoardingStairs& boardingStairs : passengerBoardingStairs)
            boardingStairs.Draw();
        for (const FlightPlan& plan : flights)
            if (plan.spawned && plan.airplane.state != AirplaneState::Departed)
                plan.airplane.Draw(plan.airplane.renderScale);
        staticGate1Aircraft.Draw(staticGate1Aircraft.renderScale);
        staticEastWingAircraft.Draw(staticEastWingAircraft.renderScale);
        }
        EndMode3D();

        const Camera3D& activeCamera = sceneCamera;
        if (!showMainMenu)
        {
        const auto drawRefuellingProgress = [&](const Airplane& aircraft,
            float timeRemaining)
        {
            const Vector2 screen = ProjectAirportPoint(aircraft.position, activeCamera);
            const Vector2 center{screen.x, screen.y - 8.0f};
            const float progress = std::clamp(1.0f - timeRemaining / 6.0f,
                0.0f, 1.0f);
            DrawCircleV(center, 20.0f, Color{13, 26, 34, 230});
            DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                19.0f, Color{105, 130, 135, 255});
            constexpr int ProgressSegments = 48;
            const int completedSegments = static_cast<int>(
                std::ceil(progress * ProgressSegments));
            for (int segment = 0; segment < completedSegments; ++segment)
            {
                const float startAngle = (-90.0f +
                    360.0f * segment / ProgressSegments) * DEG2RAD;
                const float endAngle = (-90.0f +
                    360.0f * (segment + 1) / ProgressSegments) * DEG2RAD;
                DrawLineEx(
                    {center.x + std::cos(startAngle) * 17.0f,
                        center.y + std::sin(startAngle) * 17.0f},
                    {center.x + std::cos(endAngle) * 17.0f,
                        center.y + std::sin(endAngle) * 17.0f},
                    3.5f, Color{255, 200, 70, 255});
            }
            DrawText("FUEL", static_cast<int>(center.x - MeasureText("FUEL", 10) * 0.5f),
                static_cast<int>(center.y - 5.0f), 10, RAYWHITE);
        };
        if (activeRefuellingGate == "GATE_1" &&
            refuellingTankerMotion == RefuellingTankerMotion::Refuelling)
            drawRefuellingProgress(staticGate1Aircraft, gate1RefuellingTimeRemaining);
        if (activeRefuellingGate == "EAST_1" &&
            refuellingTankerMotion == RefuellingTankerMotion::Refuelling)
            drawRefuellingProgress(staticEastWingAircraft, eastWingRefuellingTimeRemaining);
        if (!selectedGateId.empty())
        {
            std::string gateLabel = selectedGateId;
            if (selectedGateId.rfind("WEST_", 0) == 0)
                gateLabel = "WEST WING GATE " + selectedGateId.substr(5);
            else if (selectedGateId.rfind("EAST_", 0) == 0)
                gateLabel = "EAST WING GATE " + selectedGateId.substr(5);
            else if (selectedGateId.rfind("GATE_", 0) == 0)
                gateLabel = "GATE " + selectedGateId.substr(5);
            const GateNavigationButtons buttons = GetGateNavigationButtons();
            DrawGateNavigationButton(buttons.previous, "PREV GATE");
            DrawGateNavigationButton(buttons.next, "NEXT GATE");
            DrawGateNavigationButton(buttons.overview, "BACK TO OVERVIEW");
            DrawGateNavigationButton(buttons.bringServiceCrew, "Bring Service Crew");
        }
        // Keep map labels compact: flight ID, next destination and a live state glyph.
        for (const FlightPlan& plan : flights)
        {
            if (!plan.spawned || plan.airplane.state == AirplaneState::Departed) continue;
            const Vector2 planeScreen = ProjectAirportPoint(plan.airplane.position, activeCamera);
            const std::string mapDestination = plan.arrivalFlight ?
                (plan.standAssigned ? plan.parkingStandId : "ARRIVAL") : plan.destination;
            const std::string text = plan.airplane.flightId + "  >  " + mapDestination;
            const int width = MeasureText(text.c_str(), 13) + 12;
            const int x = static_cast<int>(planeScreen.x + 16.0f);
            const int y = static_cast<int>(planeScreen.y - 28.0f);
            DrawRectangle(x, y, width, 22, Fade(Color{12, 27, 36, 255}, 0.91f));
            DrawText(text.c_str(), x + 6, y + 4, 13, RAYWHITE);
            DrawStateIndicator(plan.airplane.state,
                {static_cast<float>(x + width + 12), static_cast<float>(y + 11)}, 14.0f);
        }

        // Draw the selected aircraft's active or next planned taxi route over the map.
        if (selectedFlightIndex >= 0 && selectedFlightIndex < static_cast<int>(flights.size()))
        {
            const FlightPlan& selected = flights[static_cast<std::size_t>(selectedFlightIndex)];
            std::vector<std::string> routeIds = selected.activeRouteNodeIds;
            if (routeIds.empty() && selected.airplane.state == AirplaneState::AwaitingTaxiAuthorization)
                routeIds = airportGround.FindRouteNodeIds(
                    selected.landingExitNodeId, selected.parkingStandId);
            else if (routeIds.empty() && selected.airplane.state == AirplaneState::AwaitingTakeoffAuthorization)
                routeIds = airportGround.FindRouteNodeIds(
                    selected.departureHoldNodeId, selected.departureRunwayStart);
            else if (routeIds.empty() && selected.arrivalFlight)
                routeIds = airportGround.FindRouteNodeIds(
                    selected.arrivalStartNode, selected.parkingStandId);
            else if (routeIds.empty() && selected.departureFlight &&
                selected.airplane.state != AirplaneState::Departed)
                routeIds = airportGround.FindRouteNodeIds(
                    selected.parkingStandId, selected.departureHoldNodeId);
            Vector2 previous{};
            bool havePrevious = false;
            for (const std::string& nodeId : routeIds)
            {
                const TaxiwayNode* node = airportGround.FindNode(nodeId);
                if (node == nullptr) continue;
                const Vector2 point = GetWorldToScreen(
                    {node->position.x, 1.5f, node->position.z}, activeCamera);
                if (havePrevious)
                    DrawLineEx(previous, point, 4.0f, Color{48, 227, 198, 235});
                previous = point;
                havePrevious = true;
            }
            const Vector2 aircraftScreen = ProjectAirportPoint(
                selected.airplane.position, activeCamera);
            DrawCircleLines(static_cast<int>(aircraftScreen.x),
                static_cast<int>(aircraftScreen.y), 25.0f, Color{74, 238, 216, 255});
        }

        DrawText("Pan: WASD or arrow keys    Zoom: mouse wheel", 18, 16, 18, RAYWHITE);
        DrawText(
            TextFormat("Zoom: %.2fx   Render: %d x %d",
                cameraController.GetZoomLevel(), GetRenderWidth(), GetRenderHeight()),
            18, 42, 16, Color{185, 201, 207, 255});

        DrawText(TextFormat("SHIFT %s", FormatShiftTime(shiftClockSeconds).c_str()),
            GetScreenWidth() - 190, 16, 18, RAYWHITE);
        const int panelX = 16;
        const int panelY = 74;
        const int rowHeight = 21;
        DrawRectangle(panelX, panelY, 300, 35 + static_cast<int>(flights.size()) * rowHeight,
            Fade(Color{13, 26, 34, 255}, 0.88f));
        DrawRectangle(panelX, panelY, 300, 28, Color{33, 66, 82, 255});
        DrawText("FLIGHTS          DESTINATION", panelX + 9, panelY + 7, 13, RAYWHITE);
        for (std::size_t i = 0; i < flights.size(); ++i)
        {
            const FlightPlan& plan = flights[i];
            const std::string mapDestination = !plan.spawned ? plan.destination :
                (plan.arrivalFlight ? (plan.standAssigned ? plan.parkingStandId : "ARRIVAL") : plan.destination);
            const Color rowColor = plan.spawned && plan.airplane.state != AirplaneState::Departed ?
                Color{235, 242, 245, 255} : Color{166, 184, 191, 255};
            const int rowY = panelY + 34 + static_cast<int>(i) * rowHeight;
            DrawText(TextFormat("%s", plan.airplane.flightId.c_str()), panelX + 12, rowY, 14, rowColor);
            DrawText(mapDestination.c_str(), panelX + 105, rowY, 14, rowColor);
            const Vector2 indicator{static_cast<float>(panelX + 280), static_cast<float>(rowY + 8)};
            if (plan.spawned) DrawStateIndicator(plan.airplane.state, indicator, 12.0f);
            else
            {
                DrawCircleLines(static_cast<int>(indicator.x), static_cast<int>(indicator.y),
                    5.0f, Color{150, 169, 177, 255});
                DrawLineEx({indicator.x, indicator.y - 3.0f}, indicator, 1.5f,
                    Color{150, 169, 177, 255});
                DrawLineEx(indicator, {indicator.x + 2.5f, indicator.y + 1.5f}, 1.5f,
                    Color{150, 169, 177, 255});
            }
        }

        // Show the details panel only when an aircraft is selected.
        if (selectedFlightIndex >= 0 && selectedFlightIndex < static_cast<int>(flights.size()))
        {
            const int infoX = GetScreenWidth() - 326;
            const int infoY = 74;
            const FlightPlan& selected = flights[static_cast<std::size_t>(selectedFlightIndex)];
            DrawRectangle(infoX, infoY, 312, 324, Fade(Color{13, 26, 34, 255}, 0.93f));
            DrawRectangle(infoX, infoY, 312, 28, Color{33, 66, 82, 255});
            DrawText("FLIGHT DETAILS", infoX + 12, infoY + 7, 15, RAYWHITE);
            DrawAirplanePreview3D(selected.airplane,
                {static_cast<float>(infoX + 14), static_cast<float>(infoY + 40), 128.0f, 112.0f},
                airplanePreviewCamera, airplanePreviewTarget);
            DrawText(TextFormat("ID: %s", selected.airplane.flightId.c_str()), infoX + 154, infoY + 40, 15, RAYWHITE);
            DrawText(TextFormat("Name: %s", selected.flightName.c_str()), infoX + 154, infoY + 64, 13, Color{203, 219, 224, 255});
            DrawText(TextFormat("Type: %s", selected.airplane.aircraftType.c_str()), infoX + 154, infoY + 85, 12, Color{203, 219, 224, 255});
            DrawText(TextFormat("Airline: %s", selected.airline.c_str()), infoX + 14, infoY + 133, 14, Color{203, 219, 224, 255});
            DrawText(TextFormat("State: %s", FlightStateLabel(selected.airplane.state)), infoX + 14, infoY + 157, 14, Color{203, 219, 224, 255});
            DrawText(TextFormat("Schedule ARR %s  DEP %s",
                selected.hasArrivalSchedule ? FormatShiftTime(selected.arrivalScheduledSeconds).c_str() : "--:--",
                selected.departureFlight ? FormatShiftTime(selected.departureScheduledSeconds).c_str() : "--:--"),
                infoX + 14, infoY + 181, 12, Color{203, 219, 224, 255});
            DrawText(TextFormat("Route: %s  >  %s", selected.origin.c_str(), selected.destination.c_str()),
                infoX + 14, infoY + 205, 12, Color{116, 226, 202, 255});
            DrawText(TextFormat("Assigned stand: %s", selected.standAssigned ? selected.parkingStandId.c_str() : "Pending assignment"),
                infoX + 14, infoY + 228, 12, Color{203, 219, 224, 255});
            const char* command = selected.airplane.state == AirplaneState::AwaitingTaxiAuthorization ?
                (selected.taxiToStandQueued ? "TAXI REQUEST QUEUED" : "AUTHORIZE TAXI TO STAND") :
                (selected.airplane.state == AirplaneState::Parked && selected.departureFlight ?
                    "AUTHORIZE TAXI TO RUNWAY" :
                 selected.airplane.state == AirplaneState::AwaitingTakeoffAuthorization ?
                    "AUTHORIZE TAKEOFF" : "No taxi command available in this state.");
            DrawRectangle(infoX + 14, infoY + 250, 284, 24, Color{28, 92, 98, 255});
            DrawText(command, infoX + 22, infoY + 256, 11, RAYWHITE);
            DrawRectangle(infoX + 14, infoY + 278, 284, 28,
                selected.airplane.isPriority ? Color{126, 91, 36, 255} : Color{48, 74, 88, 255});
            DrawText(selected.airplane.isPriority ? "PRIORITY: ON (click / P)" : "MARK PRIORITY (click / P)",
                infoX + 22, infoY + 285, 13, RAYWHITE);
        }

        if (!selectedGateId.empty())
        {
            std::string gateLabel = selectedGateId;
            if (selectedGateId.rfind("WEST_", 0) == 0)
                gateLabel = "WEST WING GATE " + selectedGateId.substr(5);
            else if (selectedGateId.rfind("EAST_", 0) == 0)
                gateLabel = "EAST WING GATE " + selectedGateId.substr(5);
            else if (selectedGateId.rfind("GATE_", 0) == 0)
                gateLabel = "GATE " + selectedGateId.substr(5);
            const int labelWidth = MeasureText(gateLabel.c_str(), 22) + 32;
            const int labelX = (GetScreenWidth() - labelWidth) / 2;
            const int labelY = GetScreenHeight() - 100;
            DrawRectangle(labelX, labelY, labelWidth, 36, Color{20, 54, 67, 245});
            DrawRectangleLines(labelX, labelY, labelWidth, 36, Color{112, 211, 213, 255});
            DrawText(gateLabel.c_str(), labelX + 16, labelY + 7, 22,
                Color{105, 244, 220, 255});
        }
        }
        else
        {
            DrawMenuBackground(menuAnimationTime);
            const float screenWidth = static_cast<float>(GetScreenWidth());
            const float screenHeight = static_cast<float>(GetScreenHeight());
            const float menuX = screenWidth * 0.085f;
            const float menuY = screenHeight * 0.355f;
            const Rectangle startButton{menuX, menuY, 330.0f, 58.0f};
            const Rectangle settingsButton{menuX, menuY + 72.0f, 330.0f, 58.0f};
            const Rectangle exitButton{menuX, menuY + 144.0f, 330.0f, 58.0f};
            const Vector2 mouse = GetMousePosition();
            const bool hoverStart = CheckCollisionPointRec(mouse, startButton);
            const bool hoverSettings = CheckCollisionPointRec(mouse, settingsButton);
            const bool hoverExit = CheckCollisionPointRec(mouse, exitButton);
            const float pulse = 0.5f + 0.5f * std::sin(menuAnimationTime * 1.8f);

            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                Fade(Color{4, 16, 25, 255}, 0.18f));
            DrawRectangleGradientH(0, 0, static_cast<int>(screenWidth * 0.57f), GetScreenHeight(),
                Fade(Color{4, 16, 25, 255}, 0.96f), Fade(Color{4, 16, 25, 255}, 0.0f));
            DrawRectangleGradientV(0, static_cast<int>(screenHeight * 0.76f), GetScreenWidth(),
                static_cast<int>(screenHeight * 0.24f), Fade(Color{4, 16, 25, 255}, 0.0f),
                Fade(Color{4, 16, 25, 255}, 0.34f));
            DrawRectangle(0, 0, GetScreenWidth(), 5, Fade(Color{77, 211, 217, 255}, 0.72f));

            DrawCircle(62, 66, 24.0f, Fade(Color{39, 190, 202, 255}, 0.12f + pulse * 0.06f));
            DrawCircleLines(62, 66, 18.0f, Fade(Color{89, 227, 225, 255}, 0.78f));
            DrawText("A", 54, 53, 27, Color{120, 239, 230, 255});
            DrawText("AERODROME", 98, 51, 18, Color{221, 237, 240, 255});
            DrawText("AIRPORT OPERATIONS SIMULATOR", 98, 73, 10, Color{114, 158, 170, 255});

            DrawText("AIRPORT", static_cast<int>(menuX), static_cast<int>(screenHeight * 0.17f),
                17, Color{107, 220, 221, 255});
            DrawText("OPERATIONS", static_cast<int>(menuX) - 2,
                static_cast<int>(screenHeight * 0.205f), 45, RAYWHITE);
            DrawText("SIMULATOR", static_cast<int>(menuX) - 2,
                static_cast<int>(screenHeight * 0.267f), 45, RAYWHITE);
            DrawRectangle(static_cast<int>(menuX), static_cast<int>(screenHeight * 0.326f),
                52, 3, Color{88, 221, 219, 255});
            DrawText("DIRECT THE FLOW. KEEP THE AIRFIELD MOVING.",
                static_cast<int>(menuX), static_cast<int>(screenHeight * 0.337f),
                11, Color{180, 201, 207, 255});

            const auto drawMenuButton = [](Rectangle bounds, const char* title,
                const char* detail, bool hovered, bool primary)
            {
                const Color fill = primary ?
                    (hovered ? Color{25, 126, 144, 250} : Color{18, 88, 109, 242}) :
                    (hovered ? Color{32, 66, 80, 248} : Color{14, 38, 51, 234});
                const Color border = primary ? Color{91, 222, 218, 255} : Color{65, 115, 128, 230};
                DrawRectangleRec(bounds, fill);
                DrawRectangleLinesEx(bounds, hovered ? 2.0f : 1.0f, border);
                DrawRectangle(static_cast<int>(bounds.x), static_cast<int>(bounds.y), 4,
                    static_cast<int>(bounds.height), primary ? Color{111, 237, 227, 255} : border);
                DrawText(title, static_cast<int>(bounds.x + 23), static_cast<int>(bounds.y + 10),
                    18, RAYWHITE);
                DrawText(detail, static_cast<int>(bounds.x + 23), static_cast<int>(bounds.y + 34),
                    9, Color{166, 193, 200, 255});
                DrawText(">", static_cast<int>(bounds.x + bounds.width - 31),
                    static_cast<int>(bounds.y + 18), 18,
                    hovered ? Color{137, 244, 229, 255} : Color{111, 171, 183, 255});
            };
            drawMenuButton(startButton, "START GAME", "OPEN AIRFIELD CONTROL", hoverStart, true);
            drawMenuButton(settingsButton, "SETTINGS", "COMING SOON", hoverSettings, false);
            drawMenuButton(exitButton, "EXIT", "CLOSE SIMULATOR", hoverExit, false);

            const float statusWidth = 278.0f;
            const float statusX = screenWidth - statusWidth - 32.0f;
            DrawRectangle(static_cast<int>(statusX), 28, static_cast<int>(statusWidth), 36,
                Fade(Color{8, 28, 39, 238}, 0.92f));
            DrawCircle(static_cast<int>(statusX + 18), 46, 4.5f,
                Fade(Color{95, 234, 183, 255}, 0.68f + pulse * 0.32f));
            DrawText("AIRFIELD SYSTEMS ONLINE", static_cast<int>(statusX + 31), 39, 10,
                Color{190, 222, 222, 255});
            const int overviewY = static_cast<int>(screenHeight - 138);
            DrawRectangle(static_cast<int>(statusX), overviewY, static_cast<int>(statusWidth), 98,
                Fade(Color{7, 24, 34, 220}, 0.94f));
            DrawRectangle(static_cast<int>(statusX), overviewY, 3, 98,
                Color{71, 206, 208, 255});
            DrawText("AIRFIELD OVERVIEW", static_cast<int>(statusX + 17), overviewY + 12,
                11, Color{130, 224, 221, 255});
            DrawText(TextFormat("%02d RUNWAYS    %02d STANDS",
                static_cast<int>(airportGround.GetRunways().size()),
                static_cast<int>(terminal.GetParkingStands().size())),
                static_cast<int>(statusX + 17), overviewY + 36, 13, RAYWHITE);
            DrawText("ARRIVALS  /  DEPARTURES",
                static_cast<int>(statusX + 17), overviewY + 61, 10,
                Color{161, 185, 194, 255});
            DrawText("GROUND CONTROL",
                static_cast<int>(statusX + 17), overviewY + 77, 10,
                Color{161, 185, 194, 255});
            DrawText("v1.0  |  PRESS ENTER TO START", 22,
                GetScreenHeight() - 27, 9, Color{135, 165, 175, 255});
        }

        const auto drawRefuellingIndicator = [&](const Airplane& aircraft)
        {
            const Rectangle bounds = refuellingIndicatorBounds(aircraft);
            const Vector2 center{bounds.x + bounds.width * 0.5f,
                bounds.y + 24.0f};
            const Rectangle badge{bounds.x, bounds.y + 1.0f,
                bounds.width, 45.0f};
            DrawRectangleRounded({badge.x + 2.0f, badge.y + 3.0f,
                badge.width, badge.height}, 0.35f, 8, Color{8, 19, 26, 170});
            DrawRectangleRounded(badge, 0.35f, 8, Color{19, 53, 65, 250});
            DrawRectangleRoundedLinesEx(badge, 0.35f, 8, 2.5f,
                Color{105, 244, 220, 255});
            DrawTriangle({center.x, center.y - 25.0f},
                {center.x - 13.0f, center.y + 2.0f},
                {center.x + 13.0f, center.y + 2.0f},
                Color{247, 184, 68, 255});
            DrawCircleV({center.x, center.y + 3.0f}, 13.0f,
                Color{247, 184, 68, 255});
            DrawTriangle({center.x, center.y - 21.0f},
                {center.x - 9.5f, center.y + 1.0f},
                {center.x + 9.5f, center.y + 1.0f},
                Color{255, 210, 82, 255});
            DrawCircleV({center.x, center.y + 3.0f}, 9.5f,
                Color{255, 210, 82, 255});
            DrawCircleV({center.x - 4.0f, center.y - 1.0f}, 2.2f,
                Color{255, 246, 203, 255});
            const int fuelLabelWidth = MeasureText("FUEL", 10);
            DrawText("FUEL", static_cast<int>(center.x - fuelLabelWidth * 0.5f),
                static_cast<int>(bounds.y + 36.0f), 10, RAYWHITE);
        };
        if (!gate1RefuellingFinished)
            drawRefuellingIndicator(staticGate1Aircraft);
        if (!eastWingRefuellingFinished)
            drawRefuellingIndicator(staticEastWingAircraft);

        if (refuellingMessageTime > 0.0f)
        {
            const int messageWidth = MeasureText(refuellingMessage.c_str(), 16) + 28;
            const int messageX = (GetScreenWidth() - messageWidth) / 2;
            DrawRectangle(messageX, 68, messageWidth, 32,
                Fade(Color{13, 26, 34, 255}, 0.94f));
            DrawRectangleLines(messageX, 68, messageWidth, 32,
                Color{112, 211, 213, 255});
            DrawText(refuellingMessage.c_str(), messageX + 14, 76, 16, RAYWHITE);
        }

        EndDrawing();
    }

    UnloadRenderTexture(airplanePreviewTarget);
    CloseWindow();
    return 0;
}
