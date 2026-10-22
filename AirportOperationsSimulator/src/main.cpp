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
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    struct BoardingPassenger
    {
        std::vector<Vector3> path;
        Vector3 position{};
        std::size_t nextWaypoint = 1;
        float walkSpeed = 4.5f;
        float animationOffset = 0.0f;
        Color shirtColor{68, 143, 178, 255};
    };

    void DrawBoardingPassenger(const BoardingPassenger& passenger, float time)
    {
        const Vector3 feet = passenger.position;
        const float stride = std::sin(time * (5.0f + passenger.walkSpeed * 0.8f) +
            passenger.animationOffset) * 0.14f;
        const Color trousers{45, 54, 62, 255};
        const Vector3 torso{feet.x, feet.y + 1.02f, feet.z};
        DrawCubeV(torso, {0.42f, 0.72f, 0.3f}, passenger.shirtColor);
        DrawSphere({feet.x, feet.y + 1.56f, feet.z}, 0.22f,
            Color{224, 190, 156, 255});
        DrawCylinderEx({feet.x - 0.11f + stride, feet.y + 0.66f, feet.z},
            {feet.x - 0.13f + stride, feet.y + 0.12f, feet.z},
            0.085f, 0.065f, 6, trousers);
        DrawCylinderEx({feet.x + 0.11f - stride, feet.y + 0.66f, feet.z},
            {feet.x + 0.13f - stride, feet.y + 0.12f, feet.z},
            0.085f, 0.065f, 6, trousers);
        DrawCylinderEx({feet.x - 0.27f, feet.y + 1.24f, feet.z},
            {feet.x - 0.39f - stride, feet.y + 0.75f, feet.z},
            0.065f, 0.05f, 6, passenger.shirtColor);
        DrawCylinderEx({feet.x + 0.27f, feet.y + 1.24f, feet.z},
            {feet.x + 0.39f + stride, feet.y + 0.75f, feet.z},
            0.065f, 0.05f, 6, passenger.shirtColor);
    }

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
        Rectangle disembarkPassengers;
        Rectangle refuelAircraft;
        Rectangle cateringAndWater;
        Rectangle loadBaggage;
        Rectangle boardPassengers;
    };

    GateNavigationButtons GetGateNavigationButtons()
    {
        const float buttonY = static_cast<float>(
            GetScreenHeight() - static_cast<int>(GateNavigationButtonHeight) - 16);
        const float actionWidth = std::min(174.0f,
            (static_cast<float>(GetScreenWidth()) -
                GateNavigationButtonMargin * 2.0f - 28.0f) / 5.0f);
        const float actionGap = 7.0f;
        const float actionHeight = 40.0f;
        const float actionRowWidth = actionWidth * 5.0f + actionGap * 4.0f;
        return {
            {GateNavigationButtonMargin, buttonY,
                GateNavigationButtonWidth, GateNavigationButtonHeight},
            {GateNavigationButtonMargin + GateNavigationButtonWidth + GateNavigationButtonGap,
                buttonY, GateNavigationButtonWidth, GateNavigationButtonHeight},
            {static_cast<float>(GetScreenWidth()) - GateNavigationButtonMargin -
                GateNavigationButtonWidth, buttonY,
                GateNavigationButtonWidth, GateNavigationButtonHeight},
            {(static_cast<float>(GetScreenWidth()) - ServiceCrewButtonWidth) * 0.5f,
                buttonY - 96.0f, ServiceCrewButtonWidth, GateNavigationButtonHeight},
            {(static_cast<float>(GetScreenWidth()) - actionRowWidth) * 0.5f,
                buttonY - actionHeight - 16.0f, actionWidth, actionHeight},
            {(static_cast<float>(GetScreenWidth()) - actionRowWidth) * 0.5f +
                actionWidth + actionGap,
                buttonY - actionHeight - 16.0f, actionWidth, actionHeight},
            {(static_cast<float>(GetScreenWidth()) - actionRowWidth) * 0.5f +
                (actionWidth + actionGap) * 2.0f,
                buttonY - actionHeight - 16.0f, actionWidth, actionHeight},
            {(static_cast<float>(GetScreenWidth()) - actionRowWidth) * 0.5f +
                (actionWidth + actionGap) * 3.0f,
                buttonY - actionHeight - 16.0f, actionWidth, actionHeight},
            {(static_cast<float>(GetScreenWidth()) - actionRowWidth) * 0.5f +
                (actionWidth + actionGap) * 4.0f,
                buttonY - actionHeight - 16.0f, actionWidth, actionHeight}
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

    enum class GateActionIcon { Fuel, Baggage, CateringAndWater, Boarding, Disembark };

    void DrawGateActionButton(Rectangle bounds, const char* label,
        const char* subtitle, float progress, Color accent, GateActionIcon icon)
    {
        const bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
        const Color border = hovered ? Color{235, 250, 247, 255} : accent;
        DrawRectangleRounded({bounds.x + 2.0f, bounds.y + 3.0f,
            bounds.width, bounds.height}, 0.22f, 8, Color{5, 14, 20, 180});
        DrawRectangleGradientV(static_cast<int>(bounds.x),
            static_cast<int>(bounds.y), static_cast<int>(bounds.width),
            static_cast<int>(bounds.height),
            hovered ? Color{38, 87, 100, 255} : Color{25, 67, 82, 255},
            Color{12, 36, 49, 255});
        DrawRectangleRoundedLinesEx(bounds, 0.22f, 8, 2.0f, border);

        const Rectangle iconBox{bounds.x + 6.0f, bounds.y + 6.0f, 30.0f, 27.0f};
        DrawRectangleRounded(iconBox, 0.22f, 6, Fade(accent, 0.18f));
        DrawRectangleRoundedLinesEx(iconBox, 0.22f, 6, 1.3f, Fade(accent, 0.75f));
        const float cx = iconBox.x + iconBox.width * 0.5f;
        if (icon == GateActionIcon::Fuel)
        {
            DrawTriangle({cx, iconBox.y + 4.0f},
                {cx - 8.0f, iconBox.y + 15.0f},
                {cx + 8.0f, iconBox.y + 15.0f}, accent);
            DrawCircleV({cx, iconBox.y + 16.0f}, 7.0f, accent);
            DrawCircleV({cx - 2.0f, iconBox.y + 13.0f}, 2.0f,
                Color{255, 248, 213, 255});
        }
        else if (icon == GateActionIcon::Baggage)
        {
            DrawRectangleLinesEx({cx - 9.0f, iconBox.y + 11.0f,
                18.0f, 12.0f}, 1.7f, accent);
            DrawLineEx({cx - 4.0f, iconBox.y + 11.0f},
                {cx - 4.0f, iconBox.y + 7.0f}, 1.7f, accent);
            DrawLineEx({cx + 4.0f, iconBox.y + 11.0f},
                {cx + 4.0f, iconBox.y + 7.0f}, 1.7f, accent);
            DrawLineEx({cx - 4.0f, iconBox.y + 7.0f},
                {cx + 4.0f, iconBox.y + 7.0f}, 1.7f, accent);
            DrawRectangle(static_cast<int>(cx - 1.0f),
                static_cast<int>(iconBox.y + 14.0f),
                3, 6, Color{255, 224, 157, 255});
        }
        else if (icon == GateActionIcon::CateringAndWater)
        {
            DrawRectangleRounded({cx - 10.0f, iconBox.y + 11.0f,
                20.0f, 10.0f}, 0.18f, 4, accent);
            DrawRectangle(static_cast<int>(cx - 6.0f),
                static_cast<int>(iconBox.y + 7.0f), 11, 6, accent);
            DrawCircleV({cx - 6.0f, iconBox.y + 22.0f}, 2.4f,
                Color{31, 41, 43, 255});
            DrawCircleV({cx + 7.0f, iconBox.y + 22.0f}, 2.4f,
                Color{31, 41, 43, 255});
            DrawCircleV({cx + 1.0f, iconBox.y + 3.0f}, 2.4f,
                Color{134, 228, 248, 255});
        }
        else if (icon == GateActionIcon::Boarding)
        {
            for (int step = 0; step < 4; ++step)
                DrawLineEx({cx - 9.0f + step * 4.0f,
                    iconBox.y + 21.0f - step * 4.0f},
                    {cx - 5.0f + step * 4.0f,
                        iconBox.y + 21.0f - step * 4.0f}, 2.0f, accent);
            DrawLineEx({cx - 9.0f, iconBox.y + 23.0f},
                {cx + 7.0f, iconBox.y + 7.0f}, 1.5f, accent);
        }
        else
        {
            DrawCircleV({cx, iconBox.y + 8.0f}, 3.4f, accent);
            DrawLineEx({cx, iconBox.y + 12.0f}, {cx, iconBox.y + 21.0f},
                2.2f, accent);
            DrawLineEx({cx - 5.0f, iconBox.y + 16.0f},
                {cx + 5.0f, iconBox.y + 16.0f}, 2.0f, accent);
            DrawLineEx({cx, iconBox.y + 19.0f},
                {cx - 4.0f, iconBox.y + 23.0f}, 1.8f, accent);
            DrawLineEx({cx, iconBox.y + 19.0f},
                {cx + 4.0f, iconBox.y + 23.0f}, 1.8f, accent);
        }

        const int labelX = static_cast<int>(bounds.x + 42.0f);
        const int textRight = static_cast<int>(bounds.x + bounds.width - 6.0f);
        int labelFontSize = 11;
        while (labelFontSize > 7 &&
            MeasureText(label, labelFontSize) > textRight - labelX)
            --labelFontSize;
        int subtitleFontSize = 8;
        while (subtitleFontSize > 6 &&
            MeasureText(subtitle, subtitleFontSize) > textRight - labelX)
            --subtitleFontSize;
        DrawText(label, labelX, static_cast<int>(bounds.y + 8.0f),
            labelFontSize, Color{244, 249, 248, 255});
        DrawText(subtitle, labelX, static_cast<int>(bounds.y + 24.0f),
            subtitleFontSize, Color{171, 204, 208, 255});
        DrawRectangle(static_cast<int>(bounds.x + 5.0f),
            static_cast<int>(bounds.y + bounds.height - 4.0f),
            static_cast<int>(bounds.width - 10.0f), 2, Color{7, 19, 27, 255});
        if (progress >= 0.0f)
            DrawRectangle(static_cast<int>(bounds.x + 5.0f),
                static_cast<int>(bounds.y + bounds.height - 4.0f),
                static_cast<int>((bounds.width - 10.0f) *
                    std::clamp(progress, 0.0f, 1.0f)),
                2, accent);
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
        bool departureScheduleReached{false};
        bool departureTaxiWindowOpen{false};
        bool spawned{false};
        bool standAssigned{false};
        bool departureFlight{false};
        bool arrivalFlight{false};
        bool routeStarted{false};
        bool departureQueued{false};
        bool taxiToStandQueued{false};
        bool runwayReserved{false};
        bool takeoffRollStarted{false};
        bool landingRouteActive{false};
        bool scoreRecorded{false};
        bool arrivalResultRecorded{false};
        bool safetyViolationRecorded{false};
        int scenarioStage{0};
        bool closureReroutePending{false};
        std::string closureRerouteStartNodeId;
        std::string activeTaskKey;
        float taskAvailableAtSeconds{-1.0f};
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
    // Edit these minute offsets to configure the shift's scheduled inbound flights.
    struct ArrivalScheduleEntry { const char* flight; const char* name; const char* airline;
        const char* origin; const char* destination; Color livery; const char* type; const char* approach;
        AircraftSize size; const char* runwayId; const char* runwayStart; const char* stand; float runwayZ;
        float arrivalMinute; float departureMinute; bool priority; int scenarioStage; };
    const ArrivalScheduleEntry arrivals[] = {
        {"AO100", "Aurora", "Northstar Air", "Reykjavik (KEF)", "Dublin (DUB)", {38, 145, 189, 255}, "Passenger jet", "APPROACH_A_E", AircraftSize::Large, "RUNWAY_A", "RUNWAY_END_E", "GATE_1", -507.0f, 0.0f, 75.0f, false, 0},
        {"AO202", "Meridian 202", "Meridian Air", "Copenhagen (CPH)", "Oslo (OSL)", {219, 124, 58, 255}, "Regional jet", "APPROACH_B_E", AircraftSize::Small, "RUNWAY_B", "RUNWAY_B_END_E", "WEST_2", -642.0f, 30.0f, 120.0f, false, 0},
        {"AO317", "Freightliner 317", "CargoLink", "Frankfurt (FRA)", "Prague (PRG)", {211, 185, 76, 255}, "Cargo jet", "APPROACH_C_E", AircraftSize::Large, "RUNWAY_C", "RUNWAY_C_END_E", "EAST_3", -777.0f, 195.0f, 300.0f, false, 1},
        {"AO512", "Skyline 512", "Skyline Airways", "Bristol (BRS)", "Amsterdam (AMS)", {98, 174, 128, 255}, "Passenger jet", "APPROACH_A_E", AircraftSize::Large, "RUNWAY_A", "RUNWAY_END_E", "GATE_2", -507.0f, 270.0f, 390.0f, false, 1},
        {"AO708", "Crosswind 708", "Coastal Air", "Edinburgh (EDI)", "Cork (ORK)", {74, 161, 126, 255}, "Passenger jet", "APPROACH_A_E", AircraftSize::Small, "RUNWAY_A", "RUNWAY_END_E", "WEST_1", -507.0f, 360.0f, 480.0f, false, 1},
        {"AO731", "Regional 731", "Meridian Air", "Manchester (MAN)", "Galway (GWY)", {182, 126, 66, 255}, "Regional jet", "APPROACH_B_E", AircraftSize::Small, "RUNWAY_B", "RUNWAY_B_END_E", "EAST_4", -642.0f, 540.0f, 630.0f, false, 1},
        {"AO900", "Evening Priority 900", "Northstar Air", "Paris (CDG)", "Dublin (DUB)", {198, 91, 67, 255}, "Passenger jet", "APPROACH_B_E", AircraftSize::Large, "RUNWAY_B", "RUNWAY_B_END_E", "EAST_2", -642.0f, 690.0f, 780.0f, true, 2}
    };
    for (const ArrivalScheduleEntry& setup : arrivals)
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
        plan.airplane.isPriority = setup.priority;
        plan.scenarioStage = setup.scenarioStage;
        plan.hasArrivalSchedule = true;
        plan.arrivalScheduledSeconds = setup.arrivalMinute * 60.0f;
        plan.departureScheduledSeconds = setup.departureMinute * 60.0f;
        plan.arrivalStartNode = setup.approach;
        plan.parkingStandId = setup.stand;
        plan.runwayId = setup.runwayId;
        plan.departureRunwayStart = setup.runwayStart;
        plan.departureHoldNodeId = std::string(setup.runwayId) == "RUNWAY_A" ?
            "A_HOLD_E" : (std::string(setup.runwayId) == "RUNWAY_B" ?
                "AB_LOWER_HOLD_E" : "BC_LOWER_HOLD_E");
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

    // Edit these entries to configure departures that begin parked at a stand.
    struct DepartureScheduleEntry
    {
        const char* flight;
        const char* name;
        const char* airline;
        const char* origin;
        const char* destination;
        Color livery;
        const char* type;
        const char* stand;
        const char* runwayId;
        const char* runwayStart;
        const char* departureHold;
        float departureMinute;
    };
    const DepartureScheduleEntry departures[] = {
        {"AO440", "Coastal 440", "Coastal Air", "Airport (HOME)", "Oslo (OSL)",
            {87, 158, 192, 255}, "Narrow-body jet", "GATE_4", "RUNWAY_A",
            "RUNWAY_END_E", "A_HOLD_E", 90.0f}
    };
    for (const DepartureScheduleEntry& setup : departures)
    {
        const TaxiwayNode* stand = airportGround.FindNode(setup.stand);
        if (stand == nullptr) continue;
        FlightPlan plan;
        plan.airplane.flightId = setup.flight;
        plan.airplane.aircraftType = setup.type;
        plan.airplane.size = AircraftSize::Small;
        plan.flightName = setup.name;
        plan.airline = setup.airline;
        plan.origin = setup.origin;
        plan.destination = setup.destination;
        plan.livery = setup.livery;
        plan.airplane.liveryColor = plan.livery;
        plan.airplane.position = {stand->position.x, stand->position.z};
        plan.airplane.headingDegrees = 180.0f;
        plan.airplane.state = AirplaneState::Parked;
        plan.airplane.renderScale = 0.17f;
        for (const AirportParkingStand& parkingStand : terminal.GetParkingStands())
        {
            if (parkingStand.id == setup.stand)
            {
                plan.parkingHeadingDegrees = parkingStand.headingDegrees;
                break;
            }
        }
        plan.departureFlight = true;
        plan.scenarioStage = 0;
        plan.departureScheduledSeconds = setup.departureMinute * 60.0f;
        plan.parkingStandId = setup.stand;
        plan.runwayId = setup.runwayId;
        plan.departureRunwayStart = setup.runwayStart;
        plan.departureHoldNodeId = setup.departureHold;
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
    int shiftScore = 0;
    int completedFlights = 0;
    int completedArrivals = 0;
    int punctualOperations = 0;
    int safetyViolations = 0;
    float totalDelaySeconds = 0.0f;
    float totalWaitingSeconds = 0.0f;
    int completedWaitTasks = 0;
    int priorityFlightsCompleted = 0;
    int priorityFlightsOnTime = 0;
    int priorityTasksHandledOnTime = 0;
    int priorityTasksHandledLate = 0;
    std::string priorityNotification;
    float priorityNotificationTime = 0.0f;
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
    enum class BaggageLoadingState { Idle, Loading, Complete };
    BaggageLoadingState gate1BaggageLoading = BaggageLoadingState::Idle;
    BaggageLoadingState eastWingBaggageLoading = BaggageLoadingState::Idle;
    float gate1BaggageLoadingTimeRemaining = 0.0f;
    float eastWingBaggageLoadingTimeRemaining = 0.0f;
    std::string activeBaggageLoadingGate;
    enum class CateringWaterState { Idle, Approaching, Working, Returning, Complete };
    CateringWaterState gate1CateringWater = CateringWaterState::Idle;
    CateringWaterState eastWingCateringWater = CateringWaterState::Idle;
    float gate1CateringWaterTimeRemaining = 0.0f;
    float eastWingCateringWaterTimeRemaining = 0.0f;
    std::string activeCateringWaterGate;
    enum class BoardingState { Idle, Boarding, ReturningStairs, Complete };
    BoardingState gate1Boarding = BoardingState::Idle;
    BoardingState eastWingBoarding = BoardingState::Idle;
    float gate1BoardingTimeRemaining = 0.0f;
    float eastWingBoardingTimeRemaining = 0.0f;
    enum class DisembarkingState { Idle, Disembarking, ReturningStairs, Complete };
    DisembarkingState gate1Disembarking = DisembarkingState::Idle;
    DisembarkingState eastWingDisembarking = DisembarkingState::Idle;
    float gate1DisembarkingTimeRemaining = 0.0f;
    float eastWingDisembarkingTimeRemaining = 0.0f;
    std::string activePassengerServiceGate;
    std::string boardingStairsGateId;
    Vector2 boardingStairsCrewPositions[2]{};
    float boardingStairsCrewHeadings[2]{};
    std::vector<Vector2> boardingStairsReturnPaths[2];
    std::size_t boardingStairsReturnWaypoints[2]{};
    std::string passengerServiceCompletionMessage;
    std::vector<BoardingPassenger> boardingPassengers;
    float boardingPassengerSpawnTimer = 0.0f;
    int boardingPassengerSpawnWaves = 0;
    Vector2 cateringTruckCrewPosition{};
    Vector2 waterTruckCrewPosition{};
    Vector2 cateringTruckApproachPosition{};
    Vector2 waterTruckApproachPosition{};
    Vector2 cateringTruckServicePosition{};
    Vector2 waterTruckServicePosition{};
    bool cateringTruckReachedApproach{false};
    bool waterTruckReachedApproach{false};
    bool cateringTruckReachedReturnLane{false};
    bool waterTruckReachedReturnLane{false};
    float cateringTruckCrewHeading = 0.0f;
    float waterTruckCrewHeading = 0.0f;
    Vector2 baggageBeltAnimationStart{};
    Vector2 baggageBeltAnimationEnd{};
    Vector2 baggageTugCrewPosition{};
    Vector2 baggageBeltLoaderCrewPosition{};
    float baggageTugCrewHeading = 0.0f;
    float baggageBeltLoaderCrewHeading = 0.0f;
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
    constexpr float SimulationSpeed = 4.0f;
    constexpr float ShiftClockRate = 15.0f; // Combined rate is 60 shift seconds per real second.
    constexpr float DepartureTaxiLeadSeconds = 25.0f * 60.0f;
    constexpr float PriorityTaskResponseSeconds = 10.0f * 60.0f;
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
    // All event times are elapsed from the displayed 06:00 shift start.
    constexpr float MorningStageEndSeconds = 3.0f * 60.0f * 60.0f; // 09:00
    constexpr float BusyStageEndSeconds = 11.0f * 60.0f * 60.0f; // 17:00
    constexpr float TaxiwayClosureStartSeconds = 4.0f * 60.0f * 60.0f; // 10:00
    constexpr float TaxiwayClosureDurationSeconds = 2.0f * 60.0f * 60.0f; // to 12:00
    const std::string closureFromNode = "A_EXIT_W";
    const std::string closureToNode = "A_CENTER";
    bool taxiwayClosureActive = false;
    bool taxiwayClosureAttempted = false;
    bool taxiwayClosureWaitingForTraffic = false;
    float taxiwayClosureEndSeconds = -1.0f;
    const auto routePurposeForTraffic = [&taxiwayClosureActive](TaxiRoutePurpose normalPurpose)
    {
        return taxiwayClosureActive ? TaxiRoutePurpose::SharedTaxiway : normalPurpose;
    };

    const auto tryRerouteForClosure = [&](FlightPlan& plan)
    {
        if (plan.landingRouteActive)
        {
            plan.closureReroutePending = false;
            return true;
        }
        if (plan.airplane.state != AirplaneState::TaxiingToStand &&
            plan.airplane.state != AirplaneState::TaxiingToRunway)
        {
            plan.closureReroutePending = false;
            return true;
        }
        if (plan.activeRouteNodeIds.empty()) return false;

        const std::string destinationNodeId = plan.activeRouteNodeIds.back();
        if (plan.closureRerouteStartNodeId.empty())
        {
            std::size_t closedEdgeIndex = plan.activeRouteNodeIds.size();
            for (std::size_t index = 0; index + 1 < plan.activeRouteNodeIds.size(); ++index)
            {
                const bool matchesForward = plan.activeRouteNodeIds[index] == closureFromNode &&
                    plan.activeRouteNodeIds[index + 1] == closureToNode;
                const bool matchesReverse = plan.activeRouteNodeIds[index] == closureToNode &&
                    plan.activeRouteNodeIds[index + 1] == closureFromNode;
                if (matchesForward || matchesReverse)
                {
                    closedEdgeIndex = index;
                    break;
                }
            }
            bool taxiwayOnlyRoute = true;
            std::size_t firstRunwayNodeIndex = plan.activeRouteNodeIds.size();
            for (std::size_t index = 0; index < plan.activeRouteNodeIds.size(); ++index)
            {
                if (!airportGround.IsRunwayOrApproachNode(
                    plan.activeRouteNodeIds[index])) continue;
                if (firstRunwayNodeIndex == plan.activeRouteNodeIds.size())
                    firstRunwayNodeIndex = index;
                if ((index > 0 && index + 1 < plan.activeRouteNodeIds.size()) ||
                    (index + 1 < plan.activeRouteNodeIds.size() &&
                        airportGround.IsRunwayOrApproachNode(
                            plan.activeRouteNodeIds[index + 1])) ||
                    (index > 0 && airportGround.IsRunwayOrApproachNode(
                        plan.activeRouteNodeIds[index - 1])))
                    taxiwayOnlyRoute = false;
            }
            if (closedEdgeIndex == plan.activeRouteNodeIds.size() && taxiwayOnlyRoute)
                return true;

            const std::size_t nextIndex = plan.airplane.GetNextWaypointIndex();
            if (closedEdgeIndex < plan.activeRouteNodeIds.size() &&
                nextIndex <= closedEdgeIndex + 1)
            {
                plan.closureRerouteStartNodeId = plan.activeRouteNodeIds[
                    std::min(nextIndex, closedEdgeIndex)];
            }
            else if (!taxiwayOnlyRoute)
            {
                const std::size_t safeNodeIndex = firstRunwayNodeIndex > 0 ?
                    firstRunwayNodeIndex - 1 : 0;
                plan.closureRerouteStartNodeId =
                    plan.activeRouteNodeIds[safeNodeIndex];
            }
            else return true;
        }

        const std::string startNodeId = plan.closureRerouteStartNodeId;
        const std::vector<std::string> oldRoute = plan.activeRouteNodeIds;
        const std::vector<std::string> newRoute = airportGround.FindRouteNodeIds(
            startNodeId, destinationNodeId,
            routePurposeForTraffic(plan.airplane.state == AirplaneState::TaxiingToStand ?
                TaxiRoutePurpose::Arrival : TaxiRoutePurpose::Departure));
        releaseRoute(oldRoute, plan.airplane.flightId);
        if (newRoute.empty() || !routeCanBeReserved(newRoute, plan.airplane.flightId))
        {
            reserveRoute(oldRoute, plan.airplane.flightId);
            plan.closureReroutePending = true;
            plan.airplane.SetWaypointRoute({plan.airplane.position}, 0.0f,
                plan.airplane.state);
            return false;
        }

        reserveRoute(newRoute, plan.airplane.flightId);
        plan.activeRouteNodeIds = newRoute;
        plan.closureReroutePending = false;
        plan.closureRerouteStartNodeId.clear();
        std::vector<Vector2> route{plan.airplane.position};
        const std::vector<Vector2> worldRoute = MakeWorldRoute(airportGround, newRoute);
        route.insert(route.end(), worldRoute.begin(), worldRoute.end());
        plan.airplane.SetWaypointRoute(route,
            plan.airplane.state == AirplaneState::TaxiingToStand ? 9.0f : 11.0f,
            plan.airplane.state == AirplaneState::TaxiingToStand ?
                AirplaneState::Parked : AirplaneState::AwaitingTakeoffAuthorization);
        return true;
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
        if (!activeBaggageLoadingGate.empty())
        {
            refuellingMessage = "Wait for baggage loading to finish.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (!activeCateringWaterGate.empty())
        {
            refuellingMessage = "Wait for catering and water service to finish.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (!activePassengerServiceGate.empty())
        {
            refuellingMessage = "Wait for passenger service to finish.";
            refuellingMessageTime = 2.5f;
            return;
        }
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
        baggageTugCrewPosition = tug;
        baggageTugCrewHeading = vehicleHeadingDegrees;
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
        baggageBeltLoaderCrewPosition = beltLoader;
        baggageBeltLoaderCrewHeading = vehicleHeadingDegrees;
        const Vector2 catering = place(placements[4], WingVehicleSpacingScale);
        cateringTrucks.front().SetPosition(catering.x, catering.y);
        cateringTrucks.back().SetHeadingDegrees(vehicleHeadingDegrees);
        cateringTruckCrewPosition = catering;
        cateringTruckCrewHeading = vehicleHeadingDegrees;
        const Vector2 water = place(placements[5], WingVehicleSpacingScale);
        waterServiceTrucks.front().SetPosition(water.x, water.y);
        waterServiceTrucks.back().SetHeadingDegrees(vehicleHeadingDegrees);
        waterTruckCrewPosition = water;
        waterTruckCrewHeading = vehicleHeadingDegrees;
        const Vector2 maintenance = place(placements[6], WingVehicleSpacingScale);
        maintenancePickupTrucks.front().SetPosition(maintenance.x, maintenance.y);
        maintenancePickupTrucks.back().SetHeadingDegrees(vehicleHeadingDegrees);
        for (std::size_t i = 7; i < 9; ++i)
        {
            const Vector2 stairs = place(placements[i], WingStairsSpacingScale);
            const std::size_t stairIndex = i - 7;
            passengerBoardingStairs[stairIndex].SetPosition(stairs.x, stairs.y);
            passengerBoardingStairs[stairIndex].SetHeadingDegrees(vehicleHeadingDegrees);
            boardingStairsCrewPositions[stairIndex] = stairs;
            boardingStairsCrewHeadings[stairIndex] = vehicleHeadingDegrees;
        }
        serviceCrewGateId = selectedGateId;
        boardingStairsGateId = selectedGateId;
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
    const auto startBaggageLoading = [&](const std::string& gateId,
        const Airplane& aircraft, bool refuellingFinished,
        bool cateringWaterFinished, bool passengersDisembarked,
        BaggageLoadingState& state, float& timeRemaining)
    {
        if (!passengersDisembarked)
        {
            refuellingMessage = "Disembark passengers before starting aircraft services.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (!refuellingFinished)
        {
            refuellingMessage = "Refuel the aircraft first, then complete catering and water before loading baggage.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (!cateringWaterFinished)
        {
            refuellingMessage = "Complete catering and water service before loading baggage.";
            refuellingMessageTime = 2.5f;
            return;
        }
        std::string missing;
        if (serviceCrewGateId != gateId || baggageTugs.empty())
            missing = "Baggage Tug";
        if (serviceCrewGateId != gateId || baggageBeltLoaders.empty())
            missing += missing.empty() ? "Belt Loader" : " and Belt Loader";
        if (!missing.empty())
        {
            refuellingMessage = "Missing " + missing + " at this gate. Bring them here first.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (state == BaggageLoadingState::Complete)
        {
            refuellingMessage = "Baggage loading is already complete.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (state == BaggageLoadingState::Loading)
        {
            refuellingMessage = "Baggage loading is already in progress.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (!activeBaggageLoadingGate.empty())
        {
            refuellingMessage = "Finish the other aircraft's baggage loading first.";
            refuellingMessageTime = 2.5f;
            return;
        }

        // Place the existing loader beside the baggage hold, with its raised end
        // pointing inward. Keep the tug and its attached carts nearby.
        const float heading = aircraft.headingDegrees;
        const float headingRadians = heading * DEG2RAD;
        const float cosine = std::cos(headingRadians);
        const float sine = std::sin(headingRadians);
        const float side = 1.0f;
        const auto aircraftLocalToWorld = [&](float localX, float localZ)
        {
            return Vector2{
                aircraft.position.x + localX * cosine - localZ * sine,
                aircraft.position.y + localX * sine + localZ * cosine};
        };
        const Vector2 loaderPosition = aircraftLocalToWorld(side * 7.0f, 7.0f);
        const Vector2 tugPosition = aircraftLocalToWorld(side * 13.0f, 18.0f);
        const float loaderHeading = std::atan2(side * cosine, side * sine) / DEG2RAD;
        baggageBeltLoaders.front().SetPosition(loaderPosition.x, loaderPosition.y);
        baggageBeltLoaders.front().SetHeadingDegrees(loaderHeading);
        baggageTugs.front().SetPosition(tugPosition.x, tugPosition.y);
        baggageTugs.front().SetHeadingDegrees(loaderHeading);

        // The loader's conveyor runs from local z=-0.65 up to z=-4.05.
        const float vehicleRadians = loaderHeading * DEG2RAD;
        const auto loaderLocalToWorld = [&](float localZ)
        {
            return Vector2{
                loaderPosition.x + localZ * std::sin(vehicleRadians),
                loaderPosition.y + localZ * std::cos(vehicleRadians)};
        };
        baggageBeltAnimationStart = loaderLocalToWorld(-0.65f);
        baggageBeltAnimationEnd = loaderLocalToWorld(-4.05f);
        state = BaggageLoadingState::Loading;
        timeRemaining = 8.0f;
        activeBaggageLoadingGate = gateId;
    };
    const auto startCateringAndWater = [&](const std::string& gateId,
        const Airplane& aircraft, bool passengersDisembarked,
        bool refuellingFinished,
        CateringWaterState& state)
    {
        if (!passengersDisembarked)
        {
            refuellingMessage = "Disembark passengers before starting aircraft services.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (!refuellingFinished)
        {
            refuellingMessage = "Refuel the aircraft before catering and water service.";
            refuellingMessageTime = 2.5f;
            return;
        }
        std::string missing;
        if (serviceCrewGateId != gateId || cateringTrucks.empty())
            missing = "Catering Truck";
        if (serviceCrewGateId != gateId || waterServiceTrucks.empty())
            missing += missing.empty() ? "Water-Service Truck" :
                " and Water-Service Truck";
        if (!missing.empty())
        {
            refuellingMessage = "Missing " + missing +
                " at this gate. Bring the service crew here first.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (state == CateringWaterState::Complete)
        {
            refuellingMessage = "Catering and water service is already complete.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (state != CateringWaterState::Idle)
        {
            refuellingMessage = "Catering and water service is already in progress.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (!activeCateringWaterGate.empty())
        {
            refuellingMessage = "Finish the other aircraft's catering and water service first.";
            refuellingMessageTime = 2.5f;
            return;
        }

        const float radians = aircraft.headingDegrees * DEG2RAD;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        const auto aircraftLocalToWorld = [&](float localX, float localZ)
        {
            return Vector2{
                aircraft.position.x + localX * cosine - localZ * sine,
                aircraft.position.y + localX * sine + localZ * cosine};
        };
        const auto worldToAircraftLocal = [&](Vector2 worldPosition)
        {
            const float dx = worldPosition.x - aircraft.position.x;
            const float dz = worldPosition.y - aircraft.position.y;
            return Vector2{dx * cosine + dz * sine,
                -dx * sine + dz * cosine};
        };
        const Vector2 cateringCrewLocal = worldToAircraftLocal(
            cateringTruckCrewPosition);
        const Vector2 waterCrewLocal = worldToAircraftLocal(
            waterTruckCrewPosition);
        const float safeSideDistance = 44.0f * aircraft.renderScale + 5.0f;
        const float safeLongitudinalDistance =
            38.0f * aircraft.renderScale + 3.0f;
        const float serviceSideOffset = 30.0f * aircraft.renderScale + 2.0f;
        const float cateringSide = cateringCrewLocal.x >= 0.0f ? 1.0f : -1.0f;
        const float waterSide = waterCrewLocal.x >= 0.0f ? 1.0f : -1.0f;
        cateringTruckApproachPosition = aircraftLocalToWorld(
            cateringSide * safeSideDistance, -safeLongitudinalDistance);
        waterTruckApproachPosition = aircraftLocalToWorld(
            waterSide * safeSideDistance, safeLongitudinalDistance);
        cateringTruckServicePosition = aircraftLocalToWorld(
            cateringSide * serviceSideOffset, -safeLongitudinalDistance);
        waterTruckServicePosition = aircraftLocalToWorld(
            waterSide * serviceSideOffset, safeLongitudinalDistance);
        cateringTruckReachedApproach = false;
        waterTruckReachedApproach = false;
        cateringTruckReachedReturnLane = false;
        waterTruckReachedReturnLane = false;
        activeCateringWaterGate = gateId;
        state = CateringWaterState::Approaching;
    };
    const auto positionBoardingStairsAtAircraft = [&](const Airplane& aircraft)
    {
        const float radians = aircraft.headingDegrees * DEG2RAD;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        const auto aircraftLocalToWorld = [&](float localX, float localZ)
        {
            return Vector2{
                aircraft.position.x + localX * cosine - localZ * sine,
                aircraft.position.y + localX * sine + localZ * cosine};
        };
        constexpr float side = 1.0f;
        constexpr float sideOffset = 2.9f;
        const float frontDoorZ = -20.0f * aircraft.renderScale;
        const float rearDoorZ = 22.0f * aircraft.renderScale;
        const float stairHeading = std::atan2(side * cosine, side * sine) / DEG2RAD;
        const Vector2 frontPosition = aircraftLocalToWorld(side * sideOffset,
            frontDoorZ);
        const Vector2 rearPosition = aircraftLocalToWorld(side * sideOffset,
            rearDoorZ);
        passengerBoardingStairs[0].SetPosition(frontPosition.x, frontPosition.y);
        passengerBoardingStairs[0].SetHeadingDegrees(stairHeading);
        passengerBoardingStairs[1].SetPosition(rearPosition.x, rearPosition.y);
        passengerBoardingStairs[1].SetHeadingDegrees(stairHeading);
    };
    const auto startBoardingStairsReturn = [&](const Airplane& aircraft)
    {
        const float radians = aircraft.headingDegrees * DEG2RAD;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        const auto toWorld = [&](float localX, float localZ)
        {
            return Vector2{
                aircraft.position.x + localX * cosine - localZ * sine,
                aircraft.position.y + localX * sine + localZ * cosine};
        };
        for (std::size_t stair = 0; stair < 2 &&
            stair < passengerBoardingStairs.size(); ++stair)
        {
            const Vector2 staging = boardingStairsCrewPositions[stair];
            const float dx = staging.x - aircraft.position.x;
            const float dz = staging.y - aircraft.position.y;
            const float targetLocalX = dx * cosine + dz * sine;
            const float targetLocalZ = -dx * sine + dz * cosine;
            const float doorZ = stair == 0 ?
                -20.0f * aircraft.renderScale : 22.0f * aircraft.renderScale;
            const float targetSide = targetLocalX >= 0.0f ? 1.0f : -1.0f;
            constexpr float clearSideDistance = 22.0f;
            const float noseClearZ = -48.0f * aircraft.renderScale;
            boardingStairsReturnPaths[stair] = {
                toWorld(clearSideDistance, doorZ),
                toWorld(clearSideDistance, noseClearZ),
                toWorld(targetSide * clearSideDistance, noseClearZ),
                toWorld(targetSide * clearSideDistance, targetLocalZ),
                staging};
            boardingStairsReturnWaypoints[stair] = 0;
        }
    };
    const auto spawnBoardingPassenger = [&](const std::string& gateId,
        const Airplane& aircraft, int stairIndex, int wave, bool reversePath)
    {
        const float radians = aircraft.headingDegrees * DEG2RAD;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        const auto toWorld = [&](float localX, float y, float localZ)
        {
            return Vector3{
                aircraft.position.x + localX * cosine - localZ * sine,
                y,
                aircraft.position.y + localX * sine + localZ * cosine};
        };
        const float sideOffset = 2.9f;
        const float doorZ = stairIndex == 0 ?
            -20.0f * aircraft.renderScale : 22.0f * aircraft.renderScale;
        // Both streams start on the terminal side, pass around the nose, and
        // follow clear lanes outside the wing and deployed service vehicles.
        const float laneX = stairIndex == 0 ? 10.5f : 12.0f;
        const float noseClearZ = -42.0f * aircraft.renderScale - 0.9f;
        Vector3 terminalExit{};
        Vector3 gateMarking{};
        if (terminal.GetGateMarkingPosition(gateId, gateMarking))
        {
            if (gateId.rfind("EAST_", 0) == 0)
                terminalExit = {104.8f - 1.0f, 0.04f, gateMarking.z};
            else if (gateId.rfind("WEST_", 0) == 0)
                terminalExit = {-104.8f + 1.0f, 0.04f, gateMarking.z};
            else
                terminalExit = {gateMarking.x, 0.04f, -3.45f};
        }
        const float exitDx = terminalExit.x - aircraft.position.x;
        const float exitDz = terminalExit.z - aircraft.position.y;
        const Vector2 terminalExitLocal{
            exitDx * cosine + exitDz * sine,
            -exitDx * sine + exitDz * cosine};
        BoardingPassenger passenger;
        passenger.path.reserve(15);
        passenger.path.push_back(toWorld(terminalExitLocal.x, 0.04f,
            terminalExitLocal.y));
        passenger.path.push_back(toWorld(laneX, 0.04f, noseClearZ));
        passenger.path.push_back(toWorld(laneX, 0.04f, doorZ));
        const float stairFootX = sideOffset + 1.42f;
        passenger.path.push_back(toWorld(sideOffset + 2.8f, 0.04f, doorZ));
        for (int step = 0; step < 8; ++step)
        {
            const float fraction = static_cast<float>(step) / 7.0f;
            passenger.path.push_back(toWorld(
                stairFootX + (sideOffset - 1.82f - stairFootX) * fraction,
                1.13f + 1.25f * fraction, doorZ));
        }
        passenger.path.push_back(toWorld(sideOffset - 1.82f, 2.55f, doorZ));
        passenger.path.push_back(toWorld(0.45f, 2.45f, doorZ));
        // Finish at the fuselage side so the passenger appears to enter the
        // correct door after climbing the matching set of stairs.
        passenger.path.push_back(toWorld(0.0f, 2.45f, doorZ));
        if (reversePath)
            std::reverse(passenger.path.begin(), passenger.path.end());
        passenger.position = passenger.path.front();
        passenger.walkSpeed = stairIndex == 0 ? 4.5f : 6.5f;
        passenger.animationOffset = static_cast<float>(wave) * 1.73f +
            static_cast<float>(stairIndex) * 0.91f;
        const Color frontPalette[] = {
            {73, 153, 184, 255}, {207, 130, 79, 255},
            {117, 161, 115, 255}, {164, 127, 181, 255}};
        const Color rearPalette[] = {
            {211, 171, 78, 255}, {81, 139, 174, 255},
            {181, 108, 103, 255}, {111, 158, 148, 255}};
        passenger.shirtColor = stairIndex == 0 ?
            frontPalette[wave % 4] : rearPalette[wave % 4];
        boardingPassengers.push_back(std::move(passenger));
    };
    const auto startPassengerBoarding = [&](const std::string& gateId,
        const Airplane& aircraft, bool passengersDisembarked,
        bool refuellingFinished,
        bool cateringWaterFinished, BaggageLoadingState baggageState,
        BoardingState& state, float& timeRemaining)
    {
        if (!passengersDisembarked)
        {
            refuellingMessage = "Disembark passengers before starting aircraft services.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (!refuellingFinished || !cateringWaterFinished ||
            baggageState != BaggageLoadingState::Complete)
        {
            refuellingMessage = "Complete refuelling, catering and water, and baggage before boarding.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (serviceCrewGateId != gateId)
        {
            refuellingMessage = "Bring the service crew with both Passenger Boarding Stairs to this gate first.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (boardingStairsGateId != gateId)
        {
            refuellingMessage = "Bring the service crew and both Passenger Boarding Stairs to this gate first.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (passengerBoardingStairs.size() < 2)
        {
            refuellingMessage = "Passenger Boarding Stairs are unavailable at this gate.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (state == BoardingState::Complete)
        {
            refuellingMessage = "Passenger boarding is already complete.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (state == BoardingState::Boarding ||
            !activePassengerServiceGate.empty())
        {
            refuellingMessage = activePassengerServiceGate == gateId ?
                "Passenger service is already in progress." :
                "Finish passenger service at the other aircraft first.";
            refuellingMessageTime = 2.5f;
            return;
        }
        positionBoardingStairsAtAircraft(aircraft);
        boardingPassengers.clear();
        boardingPassengerSpawnTimer = 0.0f;
        boardingPassengerSpawnWaves = 0;
        state = BoardingState::Boarding;
        timeRemaining = 10.0f;
        activePassengerServiceGate = gateId;
        refuellingMessage = "Passenger boarding started.";
        refuellingMessageTime = 2.5f;
    };
    const auto startPassengerDisembarking = [&](const std::string& gateId,
        const Airplane& aircraft, DisembarkingState& state,
        float& timeRemaining)
    {
        if (state == DisembarkingState::Complete)
        {
            refuellingMessage = "Passenger disembarking is already complete.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (state == DisembarkingState::Disembarking ||
            !activePassengerServiceGate.empty())
        {
            refuellingMessage = activePassengerServiceGate == gateId ?
                "Passenger service is already in progress." :
                "Finish passenger service at the other aircraft first.";
            refuellingMessageTime = 2.5f;
            return;
        }
        const bool otherServiceStarted = gateId == "GATE_1" ?
            (gate1RefuellingStarted ||
                gate1CateringWater != CateringWaterState::Idle ||
                gate1BaggageLoading != BaggageLoadingState::Idle ||
                gate1Boarding != BoardingState::Idle) :
            (eastWingRefuellingStarted ||
                eastWingCateringWater != CateringWaterState::Idle ||
                eastWingBaggageLoading != BaggageLoadingState::Idle ||
                eastWingBoarding != BoardingState::Idle);
        if (otherServiceStarted)
        {
            refuellingMessage = "Passenger disembarking must be the first service.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (serviceCrewGateId != gateId || boardingStairsGateId != gateId)
        {
            refuellingMessage = "Bring both Passenger Boarding Stairs to this gate first.";
            refuellingMessageTime = 2.5f;
            return;
        }
        if (passengerBoardingStairs.size() < 2)
        {
            refuellingMessage = "Passenger Boarding Stairs are unavailable at this gate.";
            refuellingMessageTime = 2.5f;
            return;
        }

        positionBoardingStairsAtAircraft(aircraft);
        boardingPassengers.clear();
        boardingPassengerSpawnTimer = 0.0f;
        boardingPassengerSpawnWaves = 0;
        state = DisembarkingState::Disembarking;
        timeRemaining = 10.0f;
        activePassengerServiceGate = gateId;
        refuellingMessage = "Passenger disembarking started.";
        refuellingMessageTime = 2.5f;
    };
    const auto moveServiceTruckToward = [](Vector2& current,
        Vector2 target, float deltaTime)
    {
        constexpr float ServiceTruckSpeed = 8.0f;
        const float dx = target.x - current.x;
        const float dz = target.y - current.y;
        const float distance = std::sqrt(dx * dx + dz * dz);
        const float step = ServiceTruckSpeed * deltaTime;
        if (distance <= step || distance <= 0.05f)
        {
            current = target;
            return true;
        }
        current.x += dx / distance * step;
        current.y += dz / distance * step;
        return false;
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
        const bool shiftComplete = std::all_of(flights.begin(), flights.end(),
            [](const FlightPlan& plan)
            {
                return !plan.departureFlight ||
                    plan.airplane.state == AirplaneState::Departed ||
                    plan.airplane.state == AirplaneState::Incident;
            });
        const float realDeltaTime = GetFrameTime();
        const float deltaTime = shiftComplete ? 0.0f : realDeltaTime * SimulationSpeed;
        if (exitRequested) break;
        if (showMainMenu)
            menuAnimationTime += realDeltaTime;
        else
        {
            if (!shiftComplete)
                shiftClockSeconds += deltaTime * ShiftClockRate;
            const int scenarioStage = shiftClockSeconds < MorningStageEndSeconds ? 0 :
                (shiftClockSeconds < BusyStageEndSeconds ? 1 : 2);
            if (scenarioStage == 1 && !taxiwayClosureAttempted &&
                shiftClockSeconds >= TaxiwayClosureStartSeconds)
            {
                const TaxiwayNode* closureStart = airportGround.FindNode(closureFromNode);
                const TaxiwayNode* closureEnd = airportGround.FindNode(closureToNode);
                bool sectionOccupied = false;
                if (closureStart != nullptr && closureEnd != nullptr)
                {
                    const float sx = closureEnd->position.x - closureStart->position.x;
                    const float sz = closureEnd->position.z - closureStart->position.z;
                    const float segmentLengthSquared = sx * sx + sz * sz;
                    for (const FlightPlan& plan : flights)
                    {
                        if (!plan.spawned) continue;
                        if (plan.landingRouteActive)
                        {
                            for (std::size_t i = 0;
                                i + 1 < plan.activeRouteNodeIds.size(); ++i)
                            {
                                const bool usesClosedSection =
                                    (plan.activeRouteNodeIds[i] == closureFromNode &&
                                        plan.activeRouteNodeIds[i + 1] == closureToNode) ||
                                    (plan.activeRouteNodeIds[i] == closureToNode &&
                                        plan.activeRouteNodeIds[i + 1] == closureFromNode);
                                if (usesClosedSection)
                                {
                                    sectionOccupied = true;
                                    break;
                                }
                            }
                            if (sectionOccupied) break;
                        }
                        if (plan.airplane.state != AirplaneState::TaxiingToStand &&
                            plan.airplane.state != AirplaneState::TaxiingToRunway) continue;
                        const float px = plan.airplane.position.x - closureStart->position.x;
                        const float pz = plan.airplane.position.y - closureStart->position.z;
                        const float t = segmentLengthSquared > 0.0f ?
                            std::clamp((px * sx + pz * sz) / segmentLengthSquared, 0.0f, 1.0f) : 0.0f;
                        const float dx = px - t * sx;
                        const float dz = pz - t * sz;
                        if (dx * dx + dz * dz < 11.0f * 11.0f)
                        {
                            sectionOccupied = true;
                            break;
                        }
                    }
                }
                taxiwayClosureWaitingForTraffic = sectionOccupied;
                if (!sectionOccupied && airportGround.SetTaxiwaySectionOpen(
                    closureFromNode, closureToNode, false))
                {
                    taxiwayClosureActive = true;
                    taxiwayClosureAttempted = true;
                    taxiwayClosureWaitingForTraffic = false;
                    taxiwayClosureEndSeconds = std::min(BusyStageEndSeconds,
                        shiftClockSeconds + TaxiwayClosureDurationSeconds);
                    priorityNotification = "TAXIWAY A WEST CLOSED - AIRCRAFT REROUTED";
                    priorityNotificationTime = 4.0f;
                }
            }
            if (taxiwayClosureActive && (shiftClockSeconds >= taxiwayClosureEndSeconds ||
                scenarioStage == 2))
            {
                airportGround.SetTaxiwaySectionOpen(closureFromNode, closureToNode, true);
                taxiwayClosureActive = false;
                taxiwayClosureWaitingForTraffic = false;
                priorityNotification = "TAXIWAY A WEST REOPENED";
                priorityNotificationTime = 3.0f;
            }
            for (FlightPlan& plan : flights)
            {
                if (plan.spawned && (plan.closureReroutePending ||
                    (taxiwayClosureActive && plan.routeStarted)))
                    tryRerouteForClosure(plan);
            }
            refuellingMessageTime = std::max(0.0f,
                refuellingMessageTime - realDeltaTime);
            priorityNotificationTime = std::max(0.0f,
                priorityNotificationTime - realDeltaTime);
            const auto updateBaggageLoading = [&](BaggageLoadingState& state,
                float& timeRemaining, const char* aircraftName)
            {
                if (state != BaggageLoadingState::Loading) return;
                timeRemaining = std::max(0.0f, timeRemaining - deltaTime);
                if (timeRemaining <= 0.0f)
                {
                    state = BaggageLoadingState::Complete;
                    if (!baggageTugs.empty())
                    {
                        baggageTugs.front().SetPosition(
                            baggageTugCrewPosition.x, baggageTugCrewPosition.y);
                        baggageTugs.front().SetHeadingDegrees(baggageTugCrewHeading);
                    }
                    if (!baggageBeltLoaders.empty())
                    {
                        baggageBeltLoaders.front().SetPosition(
                            baggageBeltLoaderCrewPosition.x,
                            baggageBeltLoaderCrewPosition.y);
                        baggageBeltLoaders.front().SetHeadingDegrees(
                            baggageBeltLoaderCrewHeading);
                    }
                    activeBaggageLoadingGate.clear();
                    refuellingMessage = std::string(aircraftName) +
                        " baggage loading complete.";
                    refuellingMessageTime = 3.0f;
                }
            };
            updateBaggageLoading(gate1BaggageLoading,
                gate1BaggageLoadingTimeRemaining, "Gate 1");
            updateBaggageLoading(eastWingBaggageLoading,
                eastWingBaggageLoadingTimeRemaining, "East Wing Gate 1");
            const auto updateCateringAndWater = [&](CateringWaterState& state,
                float& timeRemaining, const char* aircraftName)
            {
                if (state == CateringWaterState::Approaching)
                {
                    if (cateringTrucks.empty() || waterServiceTrucks.empty())
                        return;
                    Vector2 cateringPosition = cateringTrucks.front().GetPosition();
                    Vector2 waterPosition = waterServiceTrucks.front().GetPosition();
                    if (!cateringTruckReachedApproach)
                        cateringTruckReachedApproach = moveServiceTruckToward(
                            cateringPosition, cateringTruckApproachPosition, deltaTime);
                    else
                        moveServiceTruckToward(cateringPosition,
                            cateringTruckServicePosition, deltaTime);
                    if (!waterTruckReachedApproach)
                        waterTruckReachedApproach = moveServiceTruckToward(
                            waterPosition, waterTruckApproachPosition, deltaTime);
                    else
                        moveServiceTruckToward(waterPosition,
                            waterTruckServicePosition, deltaTime);
                    cateringTrucks.front().SetPosition(cateringPosition.x,
                        cateringPosition.y);
                    waterServiceTrucks.front().SetPosition(waterPosition.x,
                        waterPosition.y);
                    cateringTrucks.front().SetHeadingDegrees(cateringTruckCrewHeading);
                    waterServiceTrucks.front().SetHeadingDegrees(waterTruckCrewHeading);
                    const Vector2 cateringToService{
                        cateringTruckServicePosition.x - cateringPosition.x,
                        cateringTruckServicePosition.y - cateringPosition.y};
                    const Vector2 waterToService{
                        waterTruckServicePosition.x - waterPosition.x,
                        waterTruckServicePosition.y - waterPosition.y};
                    if (cateringTruckReachedApproach &&
                        waterTruckReachedApproach &&
                        cateringToService.x * cateringToService.x +
                            cateringToService.y * cateringToService.y <= 0.05f * 0.05f &&
                        waterToService.x * waterToService.x +
                            waterToService.y * waterToService.y <= 0.05f * 0.05f)
                    {
                        state = CateringWaterState::Working;
                        timeRemaining = 8.0f;
                    }
                }
                else if (state == CateringWaterState::Working)
                {
                    timeRemaining = std::max(0.0f, timeRemaining - deltaTime);
                    if (timeRemaining <= 0.0f)
                        state = CateringWaterState::Returning;
                }
                else if (state == CateringWaterState::Returning)
                {
                    if (cateringTrucks.empty() || waterServiceTrucks.empty())
                        return;
                    Vector2 cateringPosition = cateringTrucks.front().GetPosition();
                    Vector2 waterPosition = waterServiceTrucks.front().GetPosition();
                    if (!cateringTruckReachedReturnLane)
                        cateringTruckReachedReturnLane = moveServiceTruckToward(
                            cateringPosition, cateringTruckApproachPosition, deltaTime);
                    else
                        moveServiceTruckToward(cateringPosition,
                            cateringTruckCrewPosition, deltaTime);
                    if (!waterTruckReachedReturnLane)
                        waterTruckReachedReturnLane = moveServiceTruckToward(
                            waterPosition, waterTruckApproachPosition, deltaTime);
                    else
                        moveServiceTruckToward(waterPosition,
                            waterTruckCrewPosition, deltaTime);
                    cateringTrucks.front().SetPosition(cateringPosition.x,
                        cateringPosition.y);
                    waterServiceTrucks.front().SetPosition(waterPosition.x,
                        waterPosition.y);
                    const Vector2 cateringToCrew{
                        cateringTruckCrewPosition.x - cateringPosition.x,
                        cateringTruckCrewPosition.y - cateringPosition.y};
                    const Vector2 waterToCrew{
                        waterTruckCrewPosition.x - waterPosition.x,
                        waterTruckCrewPosition.y - waterPosition.y};
                    if (cateringTruckReachedReturnLane &&
                        waterTruckReachedReturnLane &&
                        cateringToCrew.x * cateringToCrew.x +
                            cateringToCrew.y * cateringToCrew.y <= 0.05f * 0.05f &&
                        waterToCrew.x * waterToCrew.x +
                            waterToCrew.y * waterToCrew.y <= 0.05f * 0.05f)
                    {
                        cateringTrucks.front().SetHeadingDegrees(
                            cateringTruckCrewHeading);
                        waterServiceTrucks.front().SetHeadingDegrees(
                            waterTruckCrewHeading);
                        state = CateringWaterState::Complete;
                        activeCateringWaterGate.clear();
                        refuellingMessage = std::string(aircraftName) +
                            " catering and water service complete.";
                        refuellingMessageTime = 3.0f;
                    }
                }
            };
            updateCateringAndWater(gate1CateringWater,
                gate1CateringWaterTimeRemaining, "Gate 1");
            updateCateringAndWater(eastWingCateringWater,
                eastWingCateringWaterTimeRemaining, "East Wing Gate 1");
            const auto advancePassengerFlow = [&](const std::string& gateId,
                const Airplane& aircraft, bool reversePath)
            {
                if (activePassengerServiceGate == gateId)
                {
                    constexpr int PassengerWaves = 14;
                    constexpr float PassengerWaveInterval = 0.38f;
                    boardingPassengerSpawnTimer -= deltaTime;
                    while (boardingPassengerSpawnWaves < PassengerWaves &&
                        boardingPassengerSpawnTimer <= 0.0f)
                    {
                        const int wave = boardingPassengerSpawnWaves++;
                        spawnBoardingPassenger(gateId, aircraft, 0, wave,
                            reversePath);
                        spawnBoardingPassenger(gateId, aircraft, 1, wave,
                            reversePath);
                        boardingPassengerSpawnTimer += PassengerWaveInterval;
                    }

                    for (auto passenger = boardingPassengers.begin();
                        passenger != boardingPassengers.end();)
                    {
                        float distanceToWalk = passenger->walkSpeed * deltaTime;
                        while (distanceToWalk > 0.0f &&
                            passenger->nextWaypoint < passenger->path.size())
                        {
                            const Vector3 target = passenger->path[
                                passenger->nextWaypoint];
                            const float dx = target.x - passenger->position.x;
                            const float dy = target.y - passenger->position.y;
                            const float dz = target.z - passenger->position.z;
                            const float distance = std::sqrt(
                                dx * dx + dy * dy + dz * dz);
                            if (distance <= distanceToWalk || distance <= 0.001f)
                            {
                                passenger->position = target;
                                distanceToWalk -= std::min(distanceToWalk, distance);
                                ++passenger->nextWaypoint;
                            }
                            else
                            {
                                const float fraction = distanceToWalk / distance;
                                passenger->position.x += dx * fraction;
                                passenger->position.y += dy * fraction;
                                passenger->position.z += dz * fraction;
                                distanceToWalk = 0.0f;
                            }
                        }
                        if (passenger->nextWaypoint >= passenger->path.size())
                            passenger = boardingPassengers.erase(passenger);
                        else
                            ++passenger;
                    }
                }
            };
            const auto finishPassengerService = [&](const char* aircraftName,
                const char* serviceName)
            {
                boardingPassengers.clear();
                passengerServiceCompletionMessage = std::string(aircraftName) +
                    " passenger " + serviceName + " complete.";
                const bool gate1 = activePassengerServiceGate == "GATE_1";
                startBoardingStairsReturn(gate1 ? staticGate1Aircraft :
                    staticEastWingAircraft);
                if (serviceName[0] == 'b')
                {
                    if (gate1) gate1Boarding = BoardingState::ReturningStairs;
                    else eastWingBoarding = BoardingState::ReturningStairs;
                }
                else
                {
                    if (gate1) gate1Disembarking =
                        DisembarkingState::ReturningStairs;
                    else eastWingDisembarking =
                        DisembarkingState::ReturningStairs;
                }
            };
            if (gate1Boarding == BoardingState::Boarding)
            {
                advancePassengerFlow("GATE_1", staticGate1Aircraft, false);
                gate1BoardingTimeRemaining = std::max(0.0f,
                    gate1BoardingTimeRemaining - deltaTime);
                if (gate1BoardingTimeRemaining <= 0.0f)
                {
                    finishPassengerService("Gate 1", "boarding");
                }
            }
            if (eastWingBoarding == BoardingState::Boarding)
            {
                advancePassengerFlow("EAST_1", staticEastWingAircraft, false);
                eastWingBoardingTimeRemaining = std::max(0.0f,
                    eastWingBoardingTimeRemaining - deltaTime);
                if (eastWingBoardingTimeRemaining <= 0.0f)
                {
                    finishPassengerService("East Wing Gate 1", "boarding");
                }
            }
            if (gate1Disembarking == DisembarkingState::Disembarking)
            {
                advancePassengerFlow("GATE_1", staticGate1Aircraft, true);
                gate1DisembarkingTimeRemaining = std::max(0.0f,
                    gate1DisembarkingTimeRemaining - deltaTime);
                if (gate1DisembarkingTimeRemaining <= 0.0f)
                {
                    finishPassengerService("Gate 1", "disembarking");
                }
            }
            if (eastWingDisembarking == DisembarkingState::Disembarking)
            {
                advancePassengerFlow("EAST_1", staticEastWingAircraft, true);
                eastWingDisembarkingTimeRemaining = std::max(0.0f,
                    eastWingDisembarkingTimeRemaining - deltaTime);
                if (eastWingDisembarkingTimeRemaining <= 0.0f)
                {
                    finishPassengerService("East Wing Gate 1", "disembarking");
                }
            }
            const auto updateBoardingStairsReturn = [&](float frameTime)
            {
                bool allReturned = passengerBoardingStairs.size() >= 2;
                for (std::size_t stair = 0; stair < 2 &&
                    stair < passengerBoardingStairs.size(); ++stair)
                {
                    std::vector<Vector2>& path = boardingStairsReturnPaths[stair];
                    std::size_t& waypoint = boardingStairsReturnWaypoints[stair];
                    if (waypoint < path.size())
                    {
                        Vector2 current = passengerBoardingStairs[stair].GetPosition();
                        const Vector2 target = path[waypoint];
                        const float dx = target.x - current.x;
                        const float dz = target.y - current.y;
                        passengerBoardingStairs[stair].SetHeadingDegrees(
                            std::atan2(-dx, -dz) / DEG2RAD);
                        if (moveServiceTruckToward(current, target, frameTime))
                            ++waypoint;
                        passengerBoardingStairs[stair].SetPosition(current.x, current.y);
                    }
                    if (waypoint < path.size()) allReturned = false;
                    else passengerBoardingStairs[stair].SetHeadingDegrees(
                        boardingStairsCrewHeadings[stair]);
                }
                if (!allReturned) return;

                const bool gate1 = activePassengerServiceGate == "GATE_1";
                if (gate1)
                {
                    if (gate1Boarding == BoardingState::ReturningStairs)
                        gate1Boarding = BoardingState::Complete;
                    if (gate1Disembarking == DisembarkingState::ReturningStairs)
                        gate1Disembarking = DisembarkingState::Complete;
                }
                else
                {
                    if (eastWingBoarding == BoardingState::ReturningStairs)
                        eastWingBoarding = BoardingState::Complete;
                    if (eastWingDisembarking == DisembarkingState::ReturningStairs)
                        eastWingDisembarking = DisembarkingState::Complete;
                }
                activePassengerServiceGate.clear();
                refuellingMessage = passengerServiceCompletionMessage;
                refuellingMessageTime = 3.0f;
            };
            if (gate1Boarding == BoardingState::ReturningStairs ||
                eastWingBoarding == BoardingState::ReturningStairs ||
                gate1Disembarking == DisembarkingState::ReturningStairs ||
                eastWingDisembarking == DisembarkingState::ReturningStairs)
                updateBoardingStairsReturn(deltaTime);
            updateRefuelling(gate1RefuellingTimeRemaining,
                gate1RefuellingFinished, "Gate 1", deltaTime);
            updateRefuelling(eastWingRefuellingTimeRemaining,
                eastWingRefuellingFinished, "East Wing Gate 1", deltaTime);
            if (refuellingTankerMotion == RefuellingTankerMotion::Refuelling &&
                ((activeRefuellingGate == "GATE_1" && gate1RefuellingFinished) ||
                    (activeRefuellingGate == "EAST_1" && eastWingRefuellingFinished)))
                refuellingTankerMotion = RefuellingTankerMotion::ReturningBehindAircraft;
            updateTankerMotion(deltaTime);
            cameraController.Update(realDeltaTime);
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
        else if (!showMainMenu && !shiftComplete &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            const Vector2 mouse = GetMousePosition();
            if (cameraController.IsGateViewActive())
            {
                const GateNavigationButtons buttons = GetGateNavigationButtons();
                if (CheckCollisionPointRec(mouse, buttons.previous))
                    selectAdjacentGate(-1);
                else if (CheckCollisionPointRec(mouse, buttons.next))
                    selectAdjacentGate(1);
                else if (CheckCollisionPointRec(mouse, buttons.bringServiceCrew))
                    bringServiceCrewToSelectedGate();
                else if (selectedGateId == "GATE_1" &&
                    CheckCollisionPointRec(mouse, buttons.disembarkPassengers))
                    startPassengerDisembarking("GATE_1", staticGate1Aircraft,
                        gate1Disembarking, gate1DisembarkingTimeRemaining);
                else if (selectedGateId == "EAST_1" &&
                    CheckCollisionPointRec(mouse, buttons.disembarkPassengers))
                    startPassengerDisembarking("EAST_1", staticEastWingAircraft,
                        eastWingDisembarking, eastWingDisembarkingTimeRemaining);
                else if (selectedGateId == "GATE_1" &&
                    CheckCollisionPointRec(mouse, buttons.refuelAircraft))
                {
                    if (gate1Disembarking != DisembarkingState::Complete)
                    {
                        refuellingMessage = "Disembark passengers before refuelling.";
                        refuellingMessageTime = 2.5f;
                    }
                    else
                    handleRefuellingClick("GATE_1", gate1RefuellingStarted,
                        gate1RefuellingFinished, gate1RefuellingTimeRemaining,
                        staticGate1Aircraft);
                }
                else if (selectedGateId == "EAST_1" &&
                    CheckCollisionPointRec(mouse, buttons.refuelAircraft))
                {
                    if (eastWingDisembarking != DisembarkingState::Complete)
                    {
                        refuellingMessage = "Disembark passengers before refuelling.";
                        refuellingMessageTime = 2.5f;
                    }
                    else
                    handleRefuellingClick("EAST_1", eastWingRefuellingStarted,
                        eastWingRefuellingFinished, eastWingRefuellingTimeRemaining,
                        staticEastWingAircraft);
                }
                else if ((selectedGateId == "GATE_1" || selectedGateId == "EAST_1") &&
                    CheckCollisionPointRec(mouse, buttons.cateringAndWater))
                {
                    const bool isGate1 = selectedGateId == "GATE_1";
                    startCateringAndWater(selectedGateId,
                        isGate1 ? staticGate1Aircraft : staticEastWingAircraft,
                        (isGate1 ? gate1Disembarking : eastWingDisembarking) ==
                            DisembarkingState::Complete,
                        isGate1 ? gate1RefuellingFinished :
                            eastWingRefuellingFinished,
                        isGate1 ? gate1CateringWater : eastWingCateringWater);
                }
                else if (selectedGateId == "GATE_1" &&
                    CheckCollisionPointRec(mouse, buttons.loadBaggage))
                    startBaggageLoading("GATE_1", staticGate1Aircraft,
                        gate1RefuellingFinished,
                        gate1CateringWater == CateringWaterState::Complete,
                        gate1Disembarking == DisembarkingState::Complete,
                        gate1BaggageLoading,
                        gate1BaggageLoadingTimeRemaining);
                else if (selectedGateId == "EAST_1" &&
                    CheckCollisionPointRec(mouse, buttons.loadBaggage))
                    startBaggageLoading("EAST_1", staticEastWingAircraft,
                        eastWingRefuellingFinished,
                        eastWingCateringWater == CateringWaterState::Complete,
                        eastWingDisembarking == DisembarkingState::Complete,
                        eastWingBaggageLoading,
                        eastWingBaggageLoadingTimeRemaining);
                else if (selectedGateId == "GATE_1" &&
                    CheckCollisionPointRec(mouse, buttons.boardPassengers))
                    startPassengerBoarding("GATE_1", staticGate1Aircraft,
                        gate1Disembarking == DisembarkingState::Complete,
                        gate1RefuellingFinished,
                        gate1CateringWater == CateringWaterState::Complete,
                        gate1BaggageLoading, gate1Boarding,
                        gate1BoardingTimeRemaining);
                else if (selectedGateId == "EAST_1" &&
                    CheckCollisionPointRec(mouse, buttons.boardPassengers))
                    startPassengerBoarding("EAST_1", staticEastWingAircraft,
                        eastWingDisembarking == DisembarkingState::Complete,
                        eastWingRefuellingFinished,
                        eastWingCateringWater == CateringWaterState::Complete,
                        eastWingBaggageLoading, eastWingBoarding,
                        eastWingBoardingTimeRemaining);
                else if (CheckCollisionPointRec(mouse, buttons.overview))
                {
                    cameraController.ReturnToOverview();
                    selectedGateId.clear();
                }
            }
            else
            {
                const bool clickedSchedulePanel =
                    mouse.x <= 406.0f && mouse.y >= 16.0f && mouse.y <= 172.0f;
                const int rightPanelLeft = GetScreenWidth() - 326;
                const bool clickedInfoPanel = selectedFlightIndex >= 0 &&
                    mouse.x >= rightPanelLeft && mouse.y >= 70.0f && mouse.y <= 398.0f;
                if (clickedInfoPanel)
                {
                    const Rectangle taxiButton{static_cast<float>(rightPanelLeft + 14), 324.0f, 284.0f, 24.0f};
                    FlightPlan& selected = flights[static_cast<std::size_t>(selectedFlightIndex)];
                    if (CheckCollisionPointRec(mouse, taxiButton))
                    {
                        if (selected.airplane.state == AirplaneState::AwaitingTaxiAuthorization &&
                            !selected.taxiToStandQueued)
                        {
                            selected.taxiToStandQueued = true;
                            selected.requestOrder = nextRequestOrder++;
                            taxiToStandQueue.push_back(static_cast<std::size_t>(selectedFlightIndex));
                        }
                        else if (selected.airplane.state == AirplaneState::Parked && selected.departureFlight &&
                            selected.departureTaxiWindowOpen && !selected.departureQueued &&
                            selected.airplane.TransitionTo(AirplaneState::PreparingForDeparture))
                            selected.preparationTime = 3.0f;
                        else if (selected.airplane.state == AirplaneState::AwaitingTakeoffAuthorization)
                        {
                            const auto runway = std::find_if(airportGround.GetRunways().begin(),
                                airportGround.GetRunways().end(), [&selected](const AirportRunway& item)
                                { return item.id == selected.runwayId; });
                            const auto route = airportGround.FindRouteNodeIds(
                                selected.departureHoldNodeId, selected.departureRunwayStart,
                                routePurposeForTraffic(TaxiRoutePurpose::Departure));
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
        for (std::size_t flightIndex = 0; flightIndex < flights.size(); ++flightIndex)
        {
            FlightPlan& plan = flights[flightIndex];
            Airplane& airplane = plan.airplane;
            if (plan.departureFlight)
            {
                if (!plan.departureScheduleReached &&
                    shiftClockSeconds >= plan.departureScheduledSeconds)
                    plan.departureScheduleReached = true;
                plan.departureTaxiWindowOpen = shiftClockSeconds >= std::max(0.0f,
                    plan.departureScheduledSeconds - DepartureTaxiLeadSeconds);
            }
            if (!plan.spawned)
            {
                if (plan.hasArrivalSchedule && shiftClockSeconds >= plan.arrivalScheduledSeconds)
                {
                    const TaxiwayNode* approach = airportGround.FindNode(plan.arrivalStartNode);
                    if (approach == nullptr) continue;
                    // Start beyond the graph's runway approach point so the
                    // landing animation has a visible final descent in view.
                    const bool arrivingFromEast = plan.arrivalStartNode.find("_E") != std::string::npos;
                    const float approachDirection = arrivingFromEast ? -1.0f : 1.0f;
                    airplane.position = {
                        approach->position.x - approachDirection * 60.0f,
                        approach->position.z};
                    airplane.altitude = 32.0f;
                    airplane.pitchDegrees = -5.0f;
                    airplane.headingDegrees = arrivingFromEast ? 270.0f : 90.0f;
                    airplane.TransitionTo(AirplaneState::Holding);
                    plan.arrivalFlight = true;
                    plan.spawned = true;
                    plan.requestOrder = nextRequestOrder++;
                    arrivalQueue.push_back(flightIndex);
                    occupiedNodes[plan.arrivalStartNode] = airplane.flightId;
                    if (airplane.isPriority)
                    {
                        priorityNotification = "PRIORITY ARRIVAL: " + airplane.flightId +
                            " - LANDING REQUEST EXPEDITED";
                        priorityNotificationTime = 4.0f;
                    }
                }
                else if (!plan.hasArrivalSchedule && plan.departureFlight &&
                    plan.departureTaxiWindowOpen)
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
            if (airplane.state == AirplaneState::Incident &&
                !plan.safetyViolationRecorded)
            {
                ++safetyViolations;
                plan.safetyViolationRecorded = true;
            }
            if (airplane.state == AirplaneState::Parked)
                airplane.headingDegrees = plan.parkingHeadingDegrees;

            std::string availableTask;
            if (airplane.state == AirplaneState::AwaitingTaxiAuthorization &&
                !plan.taxiToStandQueued)
                availableTask = "TAXI TO STAND";
            else if (airplane.state == AirplaneState::Parked && plan.departureFlight &&
                plan.departureTaxiWindowOpen && !plan.departureQueued)
                availableTask = "TAXI TO RUNWAY";
            else if (airplane.state == AirplaneState::AwaitingTakeoffAuthorization)
                availableTask = "AUTHORIZE TAKEOFF";

            if (availableTask != plan.activeTaskKey)
            {
                if (!plan.activeTaskKey.empty() && plan.taskAvailableAtSeconds >= 0.0f)
                {
                    const float responseTime = shiftClockSeconds - plan.taskAvailableAtSeconds;
                    totalWaitingSeconds += responseTime;
                    ++completedWaitTasks;
                    if (airplane.isPriority && responseTime <= PriorityTaskResponseSeconds)
                    {
                        shiftScore += 20;
                        ++priorityTasksHandledOnTime;
                    }
                    else if (airplane.isPriority)
                    {
                        shiftScore -= 10;
                        ++priorityTasksHandledLate;
                        priorityNotification = "PRIORITY " + airplane.flightId +
                            " - HANDLING TARGET MISSED";
                        priorityNotificationTime = 4.0f;
                    }
                }

                plan.activeTaskKey = availableTask;
                plan.taskAvailableAtSeconds = availableTask.empty() ? -1.0f : shiftClockSeconds;
                if (airplane.isPriority && !availableTask.empty())
                {
                    priorityNotification = "PRIORITY " + airplane.flightId +
                        " - " + availableTask + " REQUIRED";
                    priorityNotificationTime = 4.0f;
                }
            }

            if (plan.arrivalFlight && plan.routeStarted &&
                airplane.state == AirplaneState::AwaitingTaxiAuthorization)
            {
                plan.landingRouteActive = false;
                if (!plan.arrivalResultRecorded)
                {
                    const float arrivalLateness = std::max(0.0f,
                        shiftClockSeconds - plan.arrivalScheduledSeconds);
                    ++completedArrivals;
                    totalDelaySeconds += arrivalLateness;
                    if (arrivalLateness <= 300.0f)
                        ++punctualOperations;
                    plan.arrivalResultRecorded = true;
                }
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
                plan.landingRouteActive = false;
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
                if (!plan.scoreRecorded)
                {
                    const float departureLateness = std::max(0.0f,
                        shiftClockSeconds - plan.departureScheduledSeconds);
                    ++completedFlights;
                    totalDelaySeconds += departureLateness;
                    if (departureLateness <= 300.0f)
                        ++punctualOperations;
                    if (airplane.isPriority)
                    {
                        ++priorityFlightsCompleted;
                        if (departureLateness <= 120.0f)
                        {
                            shiftScore += 200;
                            ++priorityFlightsOnTime;
                        }
                        else if (departureLateness <= 300.0f)
                            shiftScore += 125;
                        else
                            shiftScore += 50;
                        priorityNotification = "PRIORITY " + airplane.flightId +
                            (departureLateness <= 120.0f ?
                                " - ON-TIME DEPARTURE BONUS" : " - DEPARTED LATE");
                        priorityNotificationTime = 4.0f;
                    }
                    else shiftScore += departureLateness <= 300.0f ? 100 : 50;
                    plan.scoreRecorded = true;
                }
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
                        // Enter from the east threshold and roll most of the
                        // runway before taking the west-side arrival exit.
                        const std::string westRunwayExit = candidate.runwayId == "RUNWAY_A" ?
                            "RWY_EXIT_W" : (candidate.runwayId == "RUNWAY_B" ?
                                "AB_LOWER_EXIT_W" : "BC_LOWER_EXIT_W");
                        std::vector<std::string> route = airportGround.FindRouteNodeIds(
                            start, westRunwayExit, TaxiRoutePurpose::Arrival);
                        const std::vector<std::string> taxiToStand =
                            airportGround.FindRouteNodeIds(westRunwayExit, stand->id,
                                routePurposeForTraffic(TaxiRoutePurpose::Arrival));
                        if (!route.empty() && !taxiToStand.empty())
                            route.insert(route.end(), taxiToStand.begin() + 1, taxiToStand.end());
                        else route.clear();
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
                    candidateRoute = airportGround.FindRouteNodeIds(start,
                        candidate.parkingStandId,
                        routePurposeForTraffic(TaxiRoutePurpose::Arrival));
                    if (candidateRoute.empty() ||
                        !routeCanBeReserved(candidateRoute, candidate.airplane.flightId)) continue;
                }
                else
                {
                    candidateRoute = airportGround.FindRouteNodeIds(
                        start, candidate.departureHoldNodeId,
                        routePurposeForTraffic(TaxiRoutePurpose::Departure));
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
                plan.landingRouteActive = true;
            }
            if (serveArrival)
            {
                plan.airplane.TransitionTo(AirplaneState::Arriving);
                plan.airplane.TransitionTo(AirplaneState::Landing);
                const std::vector<Vector2> runwayRoute = MakeWorldRoute(airportGround, routeNodeIds);
                std::vector<Vector2> route;
                route.reserve(runwayRoute.size() + 4);
                route.push_back(plan.airplane.position);
                const float approachDirection = plan.arrivalStartNode.find("_E") != std::string::npos ?
                    -1.0f : 1.0f;
                route.push_back({plan.airplane.position.x + approachDirection * 15.0f,
                    plan.airplane.position.y});
                route.push_back({plan.airplane.position.x + approachDirection * 30.0f,
                    plan.airplane.position.y});
                route.push_back({plan.airplane.position.x + approachDirection * 45.0f,
                    plan.airplane.position.y});
                route.insert(route.end(), runwayRoute.begin(), runwayRoute.end());
                const int exitWaypoint = FindRunwayExitWaypoint(
                    airportGround, routeNodeIds, plan.runwayZ);
                const std::string runwayCenterId = plan.runwayId == "RUNWAY_A" ?
                    "RUNWAY_CENTER" : plan.runwayId + "_CENTER";
                const auto runwayCenter = std::find(routeNodeIds.begin(), routeNodeIds.end(),
                    runwayCenterId);
                // Stay airborne through roughly the first half of the runway,
                // then touch down at its center before rolling to the west exit.
                const std::size_t touchdownWaypoint = runwayCenter == routeNodeIds.end() ? 5 :
                    4 + static_cast<std::size_t>(std::distance(routeNodeIds.begin(), runwayCenter));
                route.resize(static_cast<std::size_t>(exitWaypoint + 5));
                plan.airplane.SetWaypointRoute(route, 12.0f,
                    AirplaneState::AwaitingTaxiAuthorization, -1,
                    AirplaneState::TaxiingToStand, static_cast<int>(touchdownWaypoint));
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
        for (const BoardingPassenger& passenger : boardingPassengers)
            DrawBoardingPassenger(passenger, static_cast<float>(GetTime()));
        }
        EndMode3D();

        const Camera3D& activeCamera = sceneCamera;
        if (!showMainMenu)
        {
        if (!cameraController.IsGateViewActive())
        {
            const auto drawServiceNeededMarker = [&](const Airplane& aircraft)
            {
                const Vector2 aircraftScreen = ProjectAirportPoint(
                    aircraft.position, activeCamera);
                const Vector2 center{aircraftScreen.x, aircraftScreen.y - 38.0f};
                const float pulse = 0.5f + 0.5f * std::sin(
                    static_cast<float>(GetTime()) * 3.4f);
                DrawCircleV(center, 21.0f,
                    Fade(Color{255, 78, 101, 255}, 0.09f + pulse * 0.09f));
                DrawCircleV(center, 16.0f,
                    Fade(Color{255, 99, 117, 255}, 0.10f + pulse * 0.10f));
                DrawCircleV({center.x + 1.0f, center.y + 2.0f}, 12.0f,
                    Color{5, 17, 25, 220});
                DrawCircleV(center, 11.0f, Color{255, 86, 108, 255});
                DrawCircleLines(static_cast<int>(center.x),
                    static_cast<int>(center.y), 11.0f,
                    Color{255, 221, 226, 255});
                // Draw the alert glyph from shapes so it stays sharp at map scale.
                DrawRectangleRounded({center.x - 1.7f, center.y - 6.0f,
                    3.4f, 8.5f}, 0.45f, 4, Color{44, 39, 27, 255});
                DrawCircleV({center.x, center.y + 5.0f}, 1.9f,
                    Color{44, 39, 27, 255});
            };
            if (!(gate1RefuellingFinished &&
                gate1BaggageLoading == BaggageLoadingState::Complete &&
                gate1CateringWater == CateringWaterState::Complete &&
                gate1Disembarking == DisembarkingState::Complete &&
                gate1Boarding == BoardingState::Complete))
                drawServiceNeededMarker(staticGate1Aircraft);
            if (!(eastWingRefuellingFinished &&
                eastWingBaggageLoading == BaggageLoadingState::Complete &&
                eastWingCateringWater == CateringWaterState::Complete &&
                eastWingDisembarking == DisembarkingState::Complete &&
                eastWingBoarding == BoardingState::Complete))
                drawServiceNeededMarker(staticEastWingAircraft);
        }
        const bool cateringWaterActive =
            gate1CateringWater == CateringWaterState::Working ||
            eastWingCateringWater == CateringWaterState::Working;
        if (cateringWaterActive && !cateringTrucks.empty() &&
            !waterServiceTrucks.empty())
        {
            const float pulse = 0.5f + 0.5f * std::sin(
                static_cast<float>(GetTime()) * 4.0f);
            const auto drawWorkingIndicator = [&](Vector2 worldPosition,
                const char* label, Color accent)
            {
                const Vector2 screen = ProjectAirportPoint(worldPosition,
                    activeCamera);
                const Vector2 center{screen.x, screen.y - 17.0f};
                DrawCircleV(center, 10.0f,
                    Fade(accent, 0.16f + pulse * 0.12f));
                DrawCircleLines(static_cast<int>(center.x),
                    static_cast<int>(center.y), 7.0f + pulse * 2.0f, accent);
                DrawText(label, static_cast<int>(center.x - 23.0f),
                    static_cast<int>(center.y - 23.0f), 8, RAYWHITE);
            };
            drawWorkingIndicator(cateringTrucks.front().GetPosition(),
                "CATERING", Color{247, 184, 68, 255});
            drawWorkingIndicator(waterServiceTrucks.front().GetPosition(),
                "WATER", Color{107, 213, 241, 255});
        }
        const bool baggageLoadingActive =
            gate1BaggageLoading == BaggageLoadingState::Loading ||
            eastWingBaggageLoading == BaggageLoadingState::Loading;
        if (baggageLoadingActive)
        {
            const Vector2 start = ProjectAirportPoint(
                baggageBeltAnimationStart, activeCamera);
            const Vector2 end = ProjectAirportPoint(
                baggageBeltAnimationEnd, activeCamera);
            DrawLineEx(start, end, 3.0f, Color{255, 201, 91, 190});
            const float phase = static_cast<float>(GetTime()) * 0.85f;
            for (int bag = 0; bag < 3; ++bag)
            {
                const float travel = std::fmod(phase + bag * 0.34f, 1.0f);
                const Vector2 position{
                    start.x + (end.x - start.x) * travel,
                    start.y + (end.y - start.y) * travel};
                DrawRectangle(static_cast<int>(position.x - 5.0f),
                    static_cast<int>(position.y - 4.0f), 10, 8,
                    Color{205, 145, 70, 255});
                DrawRectangleLines(static_cast<int>(position.x - 5.0f),
                    static_cast<int>(position.y - 4.0f), 10, 8,
                    Color{255, 224, 157, 255});
            }
        }
        if ((gate1Boarding == BoardingState::Boarding ||
            eastWingBoarding == BoardingState::Boarding ||
            gate1Disembarking == DisembarkingState::Disembarking ||
            eastWingDisembarking == DisembarkingState::Disembarking) &&
            passengerBoardingStairs.size() >= 2)
        {
            const float pulse = 0.5f + 0.5f * std::sin(
                static_cast<float>(GetTime()) * 4.2f);
            for (const PassengerBoardingStairs& stairs : passengerBoardingStairs)
            {
                const Vector2 screen = ProjectAirportPoint(stairs.GetPosition(),
                    activeCamera);
                const Vector2 center{screen.x, screen.y - 19.0f};
                DrawCircleV(center, 8.0f,
                    Fade(Color{117, 205, 239, 255}, 0.14f + pulse * 0.13f));
                DrawCircleLines(static_cast<int>(center.x),
                    static_cast<int>(center.y), 6.0f + pulse * 2.0f,
                    Color{117, 205, 239, 255});
                const bool disembarking =
                    gate1Disembarking == DisembarkingState::Disembarking ||
                    eastWingDisembarking == DisembarkingState::Disembarking;
                const char* passengerAction = disembarking ? "EXITING" : "BOARDING";
                DrawText(passengerAction, static_cast<int>(center.x - 27.0f),
                    static_cast<int>(center.y - 20.0f), 8,
                    Color{199, 240, 248, 255});
            }
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
            const GateNavigationButtons buttons = GetGateNavigationButtons();
            DrawGateNavigationButton(buttons.previous, "PREV GATE");
            DrawGateNavigationButton(buttons.next, "NEXT GATE");
            DrawGateNavigationButton(buttons.overview, "BACK TO OVERVIEW");
            DrawGateNavigationButton(buttons.bringServiceCrew, "Bring Service Crew");
            if (selectedGateId == "GATE_1" || selectedGateId == "EAST_1")
            {
                const bool isGate1 = selectedGateId == "GATE_1";
                const bool fuelFinished = isGate1 ? gate1RefuellingFinished :
                    eastWingRefuellingFinished;
                const bool fuelStarted = isGate1 ? gate1RefuellingStarted :
                    eastWingRefuellingStarted;
                const float fuelRemaining = isGate1 ? gate1RefuellingTimeRemaining :
                    eastWingRefuellingTimeRemaining;
                const DisembarkingState disembarkingState = isGate1 ?
                    gate1Disembarking : eastWingDisembarking;
                const float disembarkingRemaining = isGate1 ?
                    gate1DisembarkingTimeRemaining :
                    eastWingDisembarkingTimeRemaining;
                const BaggageLoadingState baggageState = isGate1 ?
                    gate1BaggageLoading : eastWingBaggageLoading;
                const float baggageRemaining = isGate1 ?
                    gate1BaggageLoadingTimeRemaining :
                    eastWingBaggageLoadingTimeRemaining;
                const CateringWaterState cateringWaterState = isGate1 ?
                    gate1CateringWater : eastWingCateringWater;
                const float cateringWaterRemaining = isGate1 ?
                    gate1CateringWaterTimeRemaining :
                    eastWingCateringWaterTimeRemaining;
                const BoardingState boardingState = isGate1 ?
                    gate1Boarding : eastWingBoarding;
                const float boardingRemaining = isGate1 ?
                    gate1BoardingTimeRemaining : eastWingBoardingTimeRemaining;
                std::string disembarkLabel = "DISEMBARK PAX";
                float disembarkProgress = -1.0f;
                if (disembarkingState == DisembarkingState::Disembarking)
                {
                    disembarkProgress = std::clamp(
                        1.0f - disembarkingRemaining / 10.0f, 0.0f, 1.0f);
                    disembarkLabel = "EXITING " + std::to_string(
                        static_cast<int>(disembarkProgress * 100.0f)) + "%";
                }
                else if (disembarkingState == DisembarkingState::ReturningStairs)
                {
                    disembarkLabel = "STAIRS RETURNING";
                    disembarkProgress = 1.0f;
                }
                else if (disembarkingState == DisembarkingState::Complete)
                {
                    disembarkLabel = "DISEMBARKED";
                    disembarkProgress = 1.0f;
                }
                const Color disembarkAccent =
                    disembarkingState == DisembarkingState::Complete ?
                    Color{137, 235, 172, 255} : Color{117, 205, 239, 255};
                DrawGateActionButton(buttons.disembarkPassengers,
                    disembarkLabel.c_str(), "PASSENGERS", disembarkProgress,
                    disembarkAccent, GateActionIcon::Disembark);

                std::string fuelLabel = "REFUEL AIRCRAFT";
                float fuelProgress = -1.0f;
                if (fuelFinished)
                {
                    fuelLabel = "FUEL COMPLETE";
                    fuelProgress = 1.0f;
                }
                else if (activeRefuellingGate == selectedGateId &&
                    refuellingTankerMotion == RefuellingTankerMotion::Refuelling)
                {
                    fuelProgress = std::clamp(1.0f - fuelRemaining / 6.0f,
                        0.0f, 1.0f);
                    fuelLabel = "REFUELLING " + std::to_string(
                        static_cast<int>(fuelProgress * 100.0f)) + "%";
                }
                else if (fuelStarted)
                    fuelLabel = "TANKER EN ROUTE";
                else if (disembarkingState != DisembarkingState::Complete)
                    fuelLabel = "DISEMBARK FIRST";
                const Color fuelAccent =
                    disembarkingState == DisembarkingState::Complete ?
                    Color{247, 184, 68, 255} : Color{100, 118, 124, 255};
                DrawGateActionButton(buttons.refuelAircraft, fuelLabel.c_str(),
                    "AIRCRAFT FUEL", fuelProgress,
                    fuelAccent, GateActionIcon::Fuel);

                std::string baggageLabel = "LOAD BAGGAGE";
                float baggageProgress = -1.0f;
                if (baggageState == BaggageLoadingState::Loading)
                {
                    baggageProgress = std::clamp(1.0f - baggageRemaining / 8.0f,
                        0.0f, 1.0f);
                    baggageLabel = "LOADING " + std::to_string(
                        static_cast<int>(baggageProgress * 100.0f)) + "%";
                }
                else if (baggageState == BaggageLoadingState::Complete)
                {
                    baggageLabel = "BAGGAGE LOADED";
                    baggageProgress = 1.0f;
                }
                const Color baggageAccent = baggageState == BaggageLoadingState::Complete ?
                    Color{137, 235, 172, 255} :
                    disembarkingState == DisembarkingState::Complete ?
                        Color{105, 244, 220, 255} : Color{100, 118, 124, 255};
                DrawGateActionButton(buttons.loadBaggage, baggageLabel.c_str(),
                    "BAGGAGE HANDLING", baggageProgress, baggageAccent,
                    GateActionIcon::Baggage);

                std::string cateringWaterLabel = "CATERING + WATER";
                float cateringWaterProgress = -1.0f;
                if (cateringWaterState == CateringWaterState::Approaching)
                    cateringWaterLabel = "TRUCKS EN ROUTE";
                else if (cateringWaterState == CateringWaterState::Working)
                {
                    cateringWaterProgress = std::clamp(
                        1.0f - cateringWaterRemaining / 8.0f, 0.0f, 1.0f);
                    cateringWaterLabel = "SERVICE " + std::to_string(
                        static_cast<int>(cateringWaterProgress * 100.0f)) + "%";
                }
                else if (cateringWaterState == CateringWaterState::Returning)
                {
                    cateringWaterLabel = "RETURNING TO CREW";
                    cateringWaterProgress = 1.0f;
                }
                else if (cateringWaterState == CateringWaterState::Complete)
                {
                    cateringWaterLabel = "SERVICE COMPLETE";
                    cateringWaterProgress = 1.0f;
                }
                const Color cateringWaterAccent =
                    cateringWaterState == CateringWaterState::Complete ?
                    Color{137, 235, 172, 255} :
                    disembarkingState == DisembarkingState::Complete ?
                        Color{117, 205, 239, 255} : Color{100, 118, 124, 255};
                DrawGateActionButton(buttons.cateringAndWater,
                    cateringWaterLabel.c_str(), "CATERING + WATER",
                    cateringWaterProgress, cateringWaterAccent,
                    GateActionIcon::CateringAndWater);

                std::string boardingLabel = "BOARD PASSENGERS";
                float boardingProgress = -1.0f;
                if (boardingState == BoardingState::Boarding)
                {
                    boardingProgress = std::clamp(
                        1.0f - boardingRemaining / 10.0f, 0.0f, 1.0f);
                    boardingLabel = "BOARDING " + std::to_string(
                        static_cast<int>(boardingProgress * 100.0f)) + "%";
                }
                else if (boardingState == BoardingState::ReturningStairs)
                {
                    boardingLabel = "STAIRS RETURNING";
                    boardingProgress = 1.0f;
                }
                else if (boardingState == BoardingState::Complete)
                {
                    boardingLabel = "BOARDING DONE";
                    boardingProgress = 1.0f;
                }
                const bool boardingReady =
                    disembarkingState == DisembarkingState::Complete &&
                    fuelFinished &&
                    cateringWaterState == CateringWaterState::Complete &&
                    baggageState == BaggageLoadingState::Complete;
                const Color boardingAccent = boardingState == BoardingState::Complete ?
                    Color{137, 235, 172, 255} : boardingReady ?
                    Color{117, 205, 239, 255} : Color{100, 118, 124, 255};
                if (!boardingReady && boardingState == BoardingState::Idle)
                    boardingLabel = "SERVICES REQUIRED";
                DrawGateActionButton(buttons.boardPassengers,
                    boardingLabel.c_str(), "PASSENGER BOARDING",
                    boardingProgress, boardingAccent, GateActionIcon::Boarding);
            }
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
                    selected.landingExitNodeId, selected.parkingStandId,
                    routePurposeForTraffic(TaxiRoutePurpose::Arrival));
            else if (routeIds.empty() && selected.airplane.state == AirplaneState::AwaitingTakeoffAuthorization)
                routeIds = airportGround.FindRouteNodeIds(
                    selected.departureHoldNodeId, selected.departureRunwayStart,
                    routePurposeForTraffic(TaxiRoutePurpose::Departure));
            else if (routeIds.empty() && selected.arrivalFlight)
                routeIds = airportGround.FindRouteNodeIds(
                    selected.arrivalStartNode, selected.parkingStandId,
                    TaxiRoutePurpose::Arrival);
            else if (routeIds.empty() && selected.departureFlight &&
                selected.airplane.state != AirplaneState::Departed)
                routeIds = airportGround.FindRouteNodeIds(
                    selected.parkingStandId, selected.departureHoldNodeId,
                    routePurposeForTraffic(TaxiRoutePurpose::Departure));
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

        const int timePanelWidth = 238;
        const int timePanelHeight = 60;
        const int timePanelX = GetScreenWidth() - timePanelWidth - 18;
        const int timePanelY = 16;
        const Rectangle timePanel{static_cast<float>(timePanelX),
            static_cast<float>(timePanelY), static_cast<float>(timePanelWidth),
            static_cast<float>(timePanelHeight)};
        DrawRectangleRec(timePanel, Fade(Color{9, 25, 35, 255}, 0.94f));
        DrawRectangle(timePanelX, timePanelY, 4, timePanelHeight,
            Color{76, 224, 211, 255});
        DrawRectangleLinesEx(timePanel, 1.0f, Color{78, 145, 159, 255});
        const Vector2 clockCenter{static_cast<float>(timePanelX + 27),
            static_cast<float>(timePanelY + 30)};
        DrawCircleV(clockCenter, 12.0f, Color{26, 67, 77, 255});
        DrawCircleLines(static_cast<int>(clockCenter.x), static_cast<int>(clockCenter.y),
            11.0f, Color{86, 209, 205, 255});
        DrawLineEx(clockCenter, {clockCenter.x, clockCenter.y - 6.0f},
            2.0f, Color{200, 242, 236, 255});
        DrawLineEx(clockCenter, {clockCenter.x + 5.0f, clockCenter.y + 2.0f},
            2.0f, Color{200, 242, 236, 255});
        DrawText("LIVE SIMULATION", timePanelX + 48, timePanelY + 8, 10,
            Color{124, 204, 207, 255});
        DrawText("SIM 4X", timePanelX + timePanelWidth - 53, timePanelY + 8, 9,
            Color{124, 204, 207, 255});
        DrawText(FormatShiftTime(shiftClockSeconds).c_str(), timePanelX + 48,
            timePanelY + 24, 24, RAYWHITE);
        const int priorityFlightTotal = static_cast<int>(std::count_if(flights.begin(), flights.end(),
            [](const FlightPlan& plan) { return plan.airplane.isPriority; }));
        DrawText(TextFormat("%s %d F%d/%d P%d/%d O%d T%d/%d",
            shiftComplete ? "FINAL" : "SHIFT", shiftScore, completedFlights,
            static_cast<int>(flights.size()), priorityFlightsCompleted,
            priorityFlightTotal, priorityFlightsOnTime,
            priorityTasksHandledOnTime, priorityTasksHandledLate),
            timePanelX + 12, timePanelY + 47, 9, Color{231, 199, 119, 255});

        const int scenarioStage = shiftClockSeconds < MorningStageEndSeconds ? 0 :
            (shiftClockSeconds < BusyStageEndSeconds ? 1 : 2);
        const char* stageName = scenarioStage == 0 ? "MORNING OPERATIONS" :
            (scenarioStage == 1 ? "BUSY PERIOD" : "EVENING PRIORITY" );
        int stageFlightCount = 0;
        int stageCompletedCount = 0;
        for (const FlightPlan& plan : flights)
        {
            if (plan.scenarioStage != scenarioStage) continue;
            ++stageFlightCount;
            if (plan.airplane.state == AirplaneState::Departed) ++stageCompletedCount;
        }
        const int objectiveX = 16;
        const int objectiveY = GetScreenHeight() - 78;
        DrawRectangle(objectiveX, objectiveY, 380, 62, Fade(Color{13, 26, 34, 255}, 0.91f));
        DrawRectangle(objectiveX, objectiveY, 4, 62,
            scenarioStage == 2 ? Color{241, 185, 70, 255} : Color{76, 224, 211, 255});
        DrawText(TextFormat("SHIFT SCENARIO  %d/3  -  %s", scenarioStage + 1, stageName),
            objectiveX + 12, objectiveY + 8, 11, RAYWHITE);
        const char* objective = scenarioStage == 0 ?
            "Objective: complete the 3-flight morning wave safely." :
            (scenarioStage == 1 ?
                "Objective: handle 4 flights; Taxiway A west closes 10:00-12:00." :
                "Objective: depart priority AO900 by 19:02 for its on-time score.");
        DrawText(objective, objectiveX + 12, objectiveY + 27, 9,
            Color{202, 218, 221, 255});
        std::string scenarioStatus = TextFormat("STAGE FLIGHTS %d/%d", stageCompletedCount, stageFlightCount);
        if (scenarioStage == 1)
            scenarioStatus = taxiwayClosureActive ? "TAXIWAY A WEST CLOSED - ROUTES DIVERTED" :
                (taxiwayClosureWaitingForTraffic ? "TAXIWAY CLOSURE WAITING FOR AIRCRAFT TO CLEAR" :
                    (taxiwayClosureAttempted ? "TAXIWAY A WEST REOPENED" : "TAXIWAY A WEST CLOSURE SCHEDULED"));
        if (scenarioStage == 2 && stageCompletedCount > 0)
            scenarioStatus = priorityFlightsOnTime > 0 ?
                "PRIORITY FLIGHT COMPLETE - ON-TIME BONUS RECORDED" :
                "PRIORITY FLIGHT COMPLETE - LATE, REDUCED SCORE";
        DrawText(scenarioStatus.c_str(), objectiveX + 12, objectiveY + 44, 9,
            scenarioStage == 2 ? Color{245, 199, 104, 255} : Color{116, 226, 202, 255});

        const int panelX = 16;
        const int panelY = 16;
        constexpr int schedulePanelWidth = 380;
        constexpr int scheduleRowHeight = 24;
        const int scheduleHeaderHeight = 30;
        DrawRectangle(panelX, panelY, schedulePanelWidth,
            scheduleHeaderHeight + static_cast<int>(flights.size()) * scheduleRowHeight,
            Fade(Color{13, 26, 34, 255}, 0.88f));
        DrawRectangle(panelX, panelY, schedulePanelWidth, scheduleHeaderHeight,
            Color{33, 66, 82, 255});
        DrawText("FLIGHT SCHEDULE", panelX + 10, panelY + 8, 13, RAYWHITE);
        DrawText("NEXT EVENT", panelX + 250, panelY + 10, 10,
            Color{147, 194, 203, 255});

        std::vector<std::size_t> scheduleOrder(flights.size());
        for (std::size_t i = 0; i < scheduleOrder.size(); ++i) scheduleOrder[i] = i;
        const auto nextScheduleTime = [&](const FlightPlan& plan)
        {
            if (plan.airplane.state == AirplaneState::Departed)
                return std::numeric_limits<float>::infinity();
            if (!plan.spawned)
                return plan.hasArrivalSchedule ? plan.arrivalScheduledSeconds :
                    plan.departureScheduledSeconds;
            return plan.departureFlight ? plan.departureScheduledSeconds :
                std::numeric_limits<float>::infinity();
        };
        std::stable_sort(scheduleOrder.begin(), scheduleOrder.end(),
            [&](std::size_t left, std::size_t right)
            {
                return nextScheduleTime(flights[left]) < nextScheduleTime(flights[right]);
            });
        for (std::size_t row = 0; row < scheduleOrder.size(); ++row)
        {
            const FlightPlan& plan = flights[scheduleOrder[row]];
            const int rowY = panelY + scheduleHeaderHeight + static_cast<int>(row) * scheduleRowHeight + 4;
            const Color rowColor = plan.airplane.isPriority ? Color{248, 210, 132, 255} :
                (plan.spawned && plan.airplane.state != AirplaneState::Departed ?
                    Color{235, 242, 245, 255} : Color{166, 184, 191, 255});
            std::string event;
            if (plan.airplane.state == AirplaneState::Departed)
                event = "COMPLETE";
            else if (plan.spawned && plan.departureFlight)
                event = !plan.departureScheduleReached ?
                    "DEP " + FormatShiftTime(plan.departureScheduledSeconds) :
                    (plan.airplane.state == AirplaneState::Parked ? "DEP READY" : "DEP DUE");
            else if (plan.hasArrivalSchedule)
                event = "ARR " + FormatShiftTime(plan.arrivalScheduledSeconds);
            else
                event = "DEP " + FormatShiftTime(plan.departureScheduledSeconds);

            if (plan.airplane.isPriority)
            {
                DrawRectangle(panelX + 5, rowY, 13, 14, Color{164, 111, 36, 255});
                DrawText("P", panelX + 8, rowY + 2, 9, RAYWHITE);
            }
            DrawText(plan.airplane.flightId.c_str(), panelX + (plan.airplane.isPriority ? 23 : 10),
                rowY, 12, rowColor);
            DrawText(plan.destination.c_str(), panelX + 82, rowY, 11, rowColor);
            DrawText(event.c_str(), panelX + 250, rowY, 11,
                plan.departureScheduleReached && plan.airplane.state != AirplaneState::Departed ?
                    Color{112, 225, 196, 255} : rowColor);
            const Vector2 indicator{static_cast<float>(panelX + 363),
                static_cast<float>(rowY + 7)};
            if (plan.spawned && plan.airplane.state != AirplaneState::Departed)
                DrawStateIndicator(plan.airplane.state, indicator, 12.0f);
            else
            {
                DrawCircleLines(static_cast<int>(indicator.x), static_cast<int>(indicator.y),
                    5.0f, plan.airplane.state == AirplaneState::Departed ?
                        Color{91, 119, 127, 255} : Color{150, 169, 177, 255});
                if (plan.airplane.state != AirplaneState::Departed)
                {
                    DrawLineEx({indicator.x, indicator.y - 3.0f}, indicator, 1.5f,
                        Color{150, 169, 177, 255});
                    DrawLineEx(indicator, {indicator.x + 2.5f, indicator.y + 1.5f}, 1.5f,
                        Color{150, 169, 177, 255});
                }
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
                    (selected.departureTaxiWindowOpen ? "AUTHORIZE TAXI TO RUNWAY" :
                        TextFormat("TAXI OUT OPENS %s",
                            FormatShiftTime(std::max(0.0f, selected.departureScheduledSeconds -
                                DepartureTaxiLeadSeconds)).c_str())) :
                 selected.airplane.state == AirplaneState::AwaitingTakeoffAuthorization ?
                    "AUTHORIZE TAKEOFF" : "No taxi command available in this state.");
            std::string commandText = command;
            if (selected.airplane.isPriority && !selected.activeTaskKey.empty())
            {
                const int secondsLeft = std::max(0, static_cast<int>(std::ceil(PriorityTaskResponseSeconds -
                    (shiftClockSeconds - selected.taskAvailableAtSeconds))));
                commandText = "URGENT: " + selected.activeTaskKey + TextFormat("  %02d:%02d",
                    secondsLeft / 60, secondsLeft % 60);
            }
            DrawRectangle(infoX + 14, infoY + 250, 284, 24,
                selected.airplane.isPriority && !selected.activeTaskKey.empty() ?
                    Color{135, 83, 31, 255} : Color{28, 92, 98, 255});
            DrawText(commandText.c_str(), infoX + 22, infoY + 256, 10, RAYWHITE);
            DrawRectangle(infoX + 14, infoY + 278, 284, 28,
                selected.airplane.isPriority ? Color{126, 91, 36, 255} : Color{48, 74, 88, 255});
            DrawText(selected.airplane.isPriority ? "PRIORITY FLIGHT" : "STANDARD FLIGHT",
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
            const int labelY = GetScreenHeight() - 192;
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

        if (priorityNotificationTime > 0.0f)
        {
            const int notificationWidth = MeasureText(priorityNotification.c_str(), 13) + 28;
            const int notificationX = (GetScreenWidth() - notificationWidth) / 2;
            DrawRectangle(notificationX, 108, notificationWidth, 30,
                Fade(Color{66, 42, 18, 248}, 0.96f));
            DrawRectangleLines(notificationX, 108, notificationWidth, 30,
                Color{238, 185, 82, 255});
            DrawText(priorityNotification.c_str(), notificationX + 14, 116, 13,
                Color{255, 226, 164, 255});
        }

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

        if (shiftComplete)
        {
            const int scheduledArrivals = static_cast<int>(std::count_if(flights.begin(),
                flights.end(), [](const FlightPlan& plan) { return plan.hasArrivalSchedule; }));
            const int scheduledDepartures = static_cast<int>(std::count_if(flights.begin(),
                flights.end(), [](const FlightPlan& plan) { return plan.departureFlight; }));
            const int scheduledFlights = scheduledDepartures;
            const int totalScheduledEvents = scheduledArrivals + scheduledDepartures;
            const int completedEvents = completedArrivals + completedFlights;
            const float punctualityScore = completedEvents > 0 ?
                40.0f * static_cast<float>(punctualOperations) /
                    static_cast<float>(completedEvents) : 0.0f;
            const float completionScore = totalScheduledEvents > 0 ?
                40.0f * static_cast<float>(completedEvents) /
                    static_cast<float>(totalScheduledEvents) : 0.0f;
            const float safetyScore = scheduledFlights > 0 ?
                20.0f * std::clamp(1.0f -
                    static_cast<float>(safetyViolations) /
                        static_cast<float>(scheduledFlights), 0.0f, 1.0f) : 0.0f;
            const int finalScore = static_cast<int>(std::lround(
                punctualityScore + completionScore + safetyScore));

            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                Fade(Color{4, 12, 18, 255}, 0.82f));
            const float panelWidth = std::min(760.0f,
                static_cast<float>(GetScreenWidth()) - 32.0f);
            const float panelHeight = std::min(590.0f,
                static_cast<float>(GetScreenHeight()) - 32.0f);
            const Rectangle panel{
                (static_cast<float>(GetScreenWidth()) - panelWidth) * 0.5f,
                (static_cast<float>(GetScreenHeight()) - panelHeight) * 0.5f,
                panelWidth, panelHeight};
            DrawRectangleRounded(panel, 0.035f, 12, Color{13, 29, 39, 255});
            DrawRectangleRoundedLinesEx(panel, 0.035f, 12, 2.0f,
                Color{76, 224, 211, 255});
            DrawText("SHIFT RESULTS", static_cast<int>(panel.x + 28.0f),
                static_cast<int>(panel.y + 22.0f), 24, RAYWHITE);
            DrawText(TextFormat("OPERATIONS COMPLETE  |  SHIFT ENDED %s",
                FormatShiftTime(shiftClockSeconds).c_str()),
                static_cast<int>(panel.x + 30.0f), static_cast<int>(panel.y + 55.0f),
                11, Color{124, 204, 207, 255});

            const Rectangle scorePanel{panel.x + 24.0f, panel.y + 82.0f,
                panel.width - 48.0f, 94.0f};
            DrawRectangleRounded(scorePanel, 0.08f, 8, Color{24, 53, 64, 255});
            DrawText("FINAL SCORE", static_cast<int>(scorePanel.x + 18.0f),
                static_cast<int>(scorePanel.y + 15.0f), 11,
                Color{124, 204, 207, 255});
            DrawText(TextFormat("%d", finalScore),
                static_cast<int>(scorePanel.x + 18.0f),
                static_cast<int>(scorePanel.y + 32.0f), 38, RAYWHITE);
            DrawText("/ 100", static_cast<int>(scorePanel.x + 86.0f),
                static_cast<int>(scorePanel.y + 57.0f), 15,
                Color{161, 185, 194, 255});
            DrawText(TextFormat("PUNCTUALITY  %.0f/40     COMPLETION  %.0f/40     SAFETY  %.0f/20",
                punctualityScore, completionScore, safetyScore),
                static_cast<int>(scorePanel.x + 180.0f),
                static_cast<int>(scorePanel.y + 39.0f), 13,
                Color{222, 232, 229, 255});

            const float tileGap = 14.0f;
            const float tileWidth = (panel.width - 48.0f - tileGap) * 0.5f;
            const float tileHeight = 66.0f;
            const float tileStartX = panel.x + 24.0f;
            const float tileStartY = panel.y + 194.0f;
            const char* tileLabels[] = {
                "COMPLETED ARRIVALS", "COMPLETED DEPARTURES",
                "TOTAL DELAY", "AVG WAITING TIME (AUTH)",
                "PRIORITY FLIGHTS COMPLETED", "SAFETY VIOLATIONS"
            };
            char tileValueBuffers[6][32]{};
            std::snprintf(tileValueBuffers[0], sizeof(tileValueBuffers[0]), "%d / %d",
                completedArrivals, scheduledArrivals);
            std::snprintf(tileValueBuffers[1], sizeof(tileValueBuffers[1]), "%d / %d",
                completedFlights, scheduledDepartures);
            std::snprintf(tileValueBuffers[2], sizeof(tileValueBuffers[2]), "%.1f min",
                totalDelaySeconds / 60.0f);
            std::snprintf(tileValueBuffers[3], sizeof(tileValueBuffers[3]), "%.1f min",
                completedWaitTasks > 0 ?
                    totalWaitingSeconds / static_cast<float>(completedWaitTasks) / 60.0f : 0.0f);
            std::snprintf(tileValueBuffers[4], sizeof(tileValueBuffers[4]), "%d",
                priorityFlightsCompleted);
            std::snprintf(tileValueBuffers[5], sizeof(tileValueBuffers[5]), "%d",
                safetyViolations);
            for (int i = 0; i < 6; ++i)
            {
                const int column = i % 2;
                const int row = i / 2;
                const Rectangle tile{
                    tileStartX + static_cast<float>(column) * (tileWidth + tileGap),
                    tileStartY + static_cast<float>(row) * (tileHeight + 10.0f),
                    tileWidth, tileHeight};
                DrawRectangleRounded(tile, 0.08f, 8, Color{21, 43, 53, 255});
                DrawText(tileLabels[i], static_cast<int>(tile.x + 14.0f),
                    static_cast<int>(tile.y + 10.0f), 10,
                    Color{144, 190, 197, 255});
                DrawText(tileValueBuffers[i], static_cast<int>(tile.x + 14.0f),
                    static_cast<int>(tile.y + 31.0f), 20, RAYWHITE);
            }
            DrawText("ON-TIME OPERATIONS (within 5 min)",
                static_cast<int>(panel.x + 28.0f),
                static_cast<int>(panel.y + panel.height - 31.0f), 10,
                Color{161, 185, 194, 255});
            DrawText(TextFormat("%d / %d", punctualOperations, completedEvents),
                static_cast<int>(panel.x + panel.width - 105.0f),
                static_cast<int>(panel.y + panel.height - 33.0f), 12,
                Color{116, 226, 202, 255});
        }

        EndDrawing();
    }

    UnloadRenderTexture(airplanePreviewTarget);
    CloseWindow();
    return 0;
}
