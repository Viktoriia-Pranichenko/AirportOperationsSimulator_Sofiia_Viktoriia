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

    struct GateNavigationButtons
    {
        Rectangle previous;
        Rectangle next;
        Rectangle overview;
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
                GateNavigationButtonWidth, GateNavigationButtonHeight}
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
    AirportGround airportGround(terminal);
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

    // Preserve the temporary Gate 4 service scene and also display the parked fleet.
    BaggageTug baggageTugs[] = {{44.5f, -37.0f}};
    AircraftRefuellingTanker refuellingTankers[] = {{75.5f, -27.0f}};
    FollowMeCar followMeCars[] = {{FollowMeCarParkingPosition.x, FollowMeCarParkingPosition.y}};
    AircraftPushbackTug pushbackTugs[] = {{60.5f, -47.0f}};
    BaggageBeltLoader baggageBeltLoaders[] = {{75.5f, -13.0f}};
    CateringTruck cateringTrucks[] = {{44.5f, -13.0f}};
    WaterServiceTruck waterServiceTrucks[] = {{75.5f, -42.0f}};
    MaintenancePickupTruck maintenancePickupTrucks[] = {{44.5f, -26.0f}};
    PassengerBoardingStairs passengerBoardingStairs[] = {
        {50.0f, -17.0f}, {70.5f, -17.0f}
    };

    BaggageTug parkedBaggageTugs[] = {
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
    AircraftRefuellingTanker parkedRefuellingTankers[] = {
        {RefuellingTankerParkingPositions[0].x, RefuellingTankerParkingPositions[0].y},
        {RefuellingTankerParkingPositions[1].x, RefuellingTankerParkingPositions[1].y}
    };
    AircraftPushbackTug parkedPushbackTugs[] = {
        {PushbackTugParkingPositions[0].x, PushbackTugParkingPositions[0].y},
        {PushbackTugParkingPositions[1].x, PushbackTugParkingPositions[1].y},
        {PushbackTugParkingPositions[2].x, PushbackTugParkingPositions[2].y}
    };
    BaggageBeltLoader parkedBaggageBeltLoaders[] = {
        {BaggageBeltLoaderParkingPositions[0].x, BaggageBeltLoaderParkingPositions[0].y},
        {BaggageBeltLoaderParkingPositions[1].x, BaggageBeltLoaderParkingPositions[1].y},
        {BaggageBeltLoaderParkingPositions[2].x, BaggageBeltLoaderParkingPositions[2].y},
        {BaggageBeltLoaderParkingPositions[3].x, BaggageBeltLoaderParkingPositions[3].y},
        {BaggageBeltLoaderParkingPositions[4].x, BaggageBeltLoaderParkingPositions[4].y}
    };
    CateringTruck parkedCateringTrucks[] = {
        {CateringTruckParkingPositions[0].x, CateringTruckParkingPositions[0].y},
        {CateringTruckParkingPositions[1].x, CateringTruckParkingPositions[1].y}
    };
    WaterServiceTruck parkedWaterServiceTrucks[] = {
        {WaterServiceTruckParkingPositions[0].x, WaterServiceTruckParkingPositions[0].y},
        {WaterServiceTruckParkingPositions[1].x, WaterServiceTruckParkingPositions[1].y}
    };
    MaintenancePickupTruck parkedMaintenancePickupTrucks[] = {
        {MaintenancePickupParkingPositions[0].x, MaintenancePickupParkingPositions[0].y},
        {MaintenancePickupParkingPositions[1].x, MaintenancePickupParkingPositions[1].y}
    };
    PassengerBoardingStairs parkedPassengerBoardingStairs[] = {
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
    std::unordered_map<std::string, std::string> runwayOwners;
    std::uint64_t nextRequestOrder = 0;
    float shiftClockSeconds = 0.0f;
    int selectedFlightIndex = -1;
    std::string selectedGateId;
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

    while (!WindowShouldClose())
    {
        const float deltaTime = GetFrameTime();
        shiftClockSeconds += deltaTime * ShiftClockRate;
        cameraController.Update(deltaTime);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            const Vector2 mouse = GetMousePosition();
            if (cameraController.IsGateViewActive())
            {
                const GateNavigationButtons buttons = GetGateNavigationButtons();
                if (CheckCollisionPointRec(mouse, buttons.previous))
                    selectAdjacentGate(-1);
                else if (CheckCollisionPointRec(mouse, buttons.next))
                    selectAdjacentGate(1);
                else if (CheckCollisionPointRec(mouse, buttons.overview))
                {
                    cameraController.ReturnToOverview();
                    selectedGateId.clear();
                }
            }
            else
            {
                const int rightPanelLeft = GetScreenWidth() - 326;
                const bool clickedInfoPanel =
                    mouse.x >= rightPanelLeft && mouse.y >= 70.0f && mouse.y <= 398.0f;
                const bool clickedSchedulePanel =
                    mouse.x <= 406.0f && mouse.y >= 74.0f && mouse.y <= 214.0f;
                if (clickedInfoPanel)
                {
                    const Rectangle taxiCommandButton{
                        static_cast<float>(rightPanelLeft + 14), 324.0f, 284.0f, 24.0f};
                    const Rectangle priorityButton{
                        static_cast<float>(rightPanelLeft + 14), 352.0f, 284.0f, 28.0f};
                    if (selectedFlightIndex >= 0 &&
                        CheckCollisionPointRec(mouse, taxiCommandButton))
                    {
                        FlightPlan& selected = flights[static_cast<std::size_t>(selectedFlightIndex)];
                        if (selected.airplane.state == AirplaneState::AwaitingTaxiAuthorization &&
                            !selected.taxiToStandQueued)
                        {
                            selected.taxiToStandQueued = true;
                            selected.requestOrder = nextRequestOrder++;
                            taxiToStandQueue.push_back(static_cast<std::size_t>(selectedFlightIndex));
                        }
                        else if (selected.airplane.state == AirplaneState::Parked &&
                            selected.departureFlight && !selected.departureQueued)
                        {
                            if (selected.airplane.TransitionTo(AirplaneState::PreparingForDeparture))
                                selected.preparationTime = 3.0f;
                        }
                        else if (selected.airplane.state == AirplaneState::AwaitingTakeoffAuthorization)
                        {
                            const auto runway = std::find_if(airportGround.GetRunways().begin(),
                                airportGround.GetRunways().end(),
                                [&selected](const AirportRunway& item)
                                { return item.id == selected.runwayId; });
                            const bool runwayFree = runway != airportGround.GetRunways().end() &&
                                runway->available && runwayOwners.find(selected.runwayId) == runwayOwners.end();
                            const std::vector<std::string> runwayEntry = airportGround.FindRouteNodeIds(
                                selected.departureHoldNodeId, selected.departureRunwayStart);
                            if (runwayFree && routeCanBeReserved(runwayEntry, selected.airplane.flightId))
                            {
                                std::vector<Vector2> takeoffRoute = MakeWorldRoute(airportGround, runwayEntry);
                                if (!takeoffRoute.empty())
                                {
                                    const std::vector<Vector2> roll = MakeTakeoffRoute(airportGround,
                                        selected.runwayId, selected.departureRunwayStart,
                                        takeoffRoute.back());
                                    takeoffRoute.insert(takeoffRoute.end(), roll.begin() + 1, roll.end());
                                    const std::size_t thresholdIndex = runwayEntry.size() - 1;
                                    if (roll.size() > 1 && selected.airplane.TransitionTo(
                                        AirplaneState::TaxiingToRunway))
                                    {
                                        reserveRoute(runwayEntry, selected.airplane.flightId);
                                        selected.activeRouteNodeIds = runwayEntry;
                                        selected.routeStarted = true;
                                        selected.runwayReserved = true;
                                        selected.takeoffRollStarted = true;
                                        runwayOwners[selected.runwayId] = selected.airplane.flightId;
                                        const auto holdOwner = occupiedNodes.find(selected.departureHoldNodeId);
                                        if (holdOwner != occupiedNodes.end() &&
                                            holdOwner->second == selected.airplane.flightId)
                                            occupiedNodes.erase(holdOwner);
                                        selected.airplane.SetWaypointRoute(takeoffRoute, 72.0f,
                                            AirplaneState::Departed,
                                            static_cast<int>(thresholdIndex),
                                            AirplaneState::TakingOff);
                                    }
                                }
                            }
                        }
                    }
                    else if (selectedFlightIndex >= 0 && CheckCollisionPointRec(mouse, priorityButton))
                        flights[static_cast<std::size_t>(selectedFlightIndex)].airplane.isPriority =
                            !flights[static_cast<std::size_t>(selectedFlightIndex)].airplane.isPriority;
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

        BeginDrawing();
        ClearBackground(Color{142, 181, 199, 255});

        BeginMode3D(cameraController.GetCamera3D());
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
        EndMode3D();

        const Camera3D& activeCamera = cameraController.GetCamera3D();
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

        const int infoX = GetScreenWidth() - 326;
        const int infoY = 74;
        DrawRectangle(infoX, infoY, 312, 324, Fade(Color{13, 26, 34, 255}, 0.93f));
        DrawRectangle(infoX, infoY, 312, 28, Color{33, 66, 82, 255});
        DrawText("FLIGHT DETAILS", infoX + 12, infoY + 7, 15, RAYWHITE);
        if (selectedFlightIndex >= 0 && selectedFlightIndex < static_cast<int>(flights.size()))
        {
            const FlightPlan& selected = flights[static_cast<std::size_t>(selectedFlightIndex)];
            DrawAirplanePreview3D(selected.airplane,
                {static_cast<float>(infoX + 14), static_cast<float>(infoY + 40), 128.0f, 112.0f},
                airplanePreviewCamera, airplanePreviewTarget);
            DrawText(TextFormat("ID: %s", selected.airplane.flightId.c_str()),
                infoX + 154, infoY + 40, 15, RAYWHITE);
            DrawText(TextFormat("Name: %s", selected.flightName.c_str()),
                infoX + 154, infoY + 64, 13, Color{203, 219, 224, 255});
            DrawText(TextFormat("Type: %s", selected.airplane.aircraftType.c_str()),
                infoX + 154, infoY + 85, 12, Color{203, 219, 224, 255});
            DrawText(TextFormat("Airline: %s", selected.airline.c_str()),
                infoX + 14, infoY + 133, 14, Color{203, 219, 224, 255});
            DrawText(TextFormat("State: %s", FlightStateLabel(selected.airplane.state)),
                infoX + 14, infoY + 157, 14, Color{203, 219, 224, 255});
            DrawText(TextFormat("Schedule   ARR %s   DEP %s",
                selected.hasArrivalSchedule ? FormatShiftTime(selected.arrivalScheduledSeconds).c_str() : "--:--",
                selected.departureFlight ? FormatShiftTime(selected.departureScheduledSeconds).c_str() : "--:--"),
                infoX + 14, infoY + 181, 13, Color{203, 219, 224, 255});
            DrawText(TextFormat("Route: %s  >  %s", selected.origin.c_str(), selected.destination.c_str()),
                infoX + 14, infoY + 205, 12, Color{116, 226, 202, 255});
            DrawText(TextFormat("Assigned stand: %s",
                selected.standAssigned ? selected.parkingStandId.c_str() : "Pending assignment"),
                infoX + 14, infoY + 228, 13, Color{203, 219, 224, 255});
            const Rectangle taxiCommandButton{
                static_cast<float>(infoX + 14), static_cast<float>(infoY + 250), 284.0f, 24.0f};
            if (selected.airplane.state == AirplaneState::AwaitingTaxiAuthorization)
            {
                DrawRectangleRec(taxiCommandButton, selected.taxiToStandQueued ?
                    Color{70, 87, 88, 255} : Color{28, 122, 112, 255});
                DrawText(selected.taxiToStandQueued ? "TAXI REQUEST QUEUED" : "AUTHORIZE TAXI TO STAND",
                    infoX + 24, infoY + 256, 13, RAYWHITE);
            }
            else if (selected.airplane.state == AirplaneState::Parked &&
                selected.departureFlight)
            {
                DrawRectangleRec(taxiCommandButton, Color{28, 122, 112, 255});
                DrawText("AUTHORIZE TAXI TO RUNWAY", infoX + 24, infoY + 256, 13, RAYWHITE);
            }
            else if (selected.airplane.state == AirplaneState::AwaitingTakeoffAuthorization)
            {
                const auto runway = std::find_if(airportGround.GetRunways().begin(),
                    airportGround.GetRunways().end(),
                    [&selected](const AirportRunway& item) { return item.id == selected.runwayId; });
                const bool runwayFree = runway != airportGround.GetRunways().end() &&
                    runway->available && runwayOwners.find(selected.runwayId) == runwayOwners.end();
                DrawRectangleRec(taxiCommandButton, runwayFree ?
                    Color{28, 122, 112, 255} : Color{70, 87, 88, 255});
                DrawText(runwayFree ? "AUTHORIZE TAKEOFF" : "RUNWAY UNAVAILABLE",
                    infoX + 24, infoY + 256, 13, RAYWHITE);
            }
            else if (selected.airplane.state == AirplaneState::PreparingForDeparture)
                DrawText("PREPARING FOR TAXI OUT...", infoX + 14, infoY + 256, 11,
                    Color{145, 166, 174, 255});
            else
                DrawText("No taxi command available in this state.",
                    infoX + 14, infoY + 256, 11, Color{145, 166, 174, 255});
            const Rectangle priorityButton{
                static_cast<float>(infoX + 14), static_cast<float>(infoY + 278), 284.0f, 28.0f};
            DrawRectangleRec(priorityButton, selected.airplane.isPriority ?
                Color{126, 91, 36, 255} : Color{48, 74, 88, 255});
            DrawText(selected.airplane.isPriority ? "PRIORITY: ON  (click / P)" : "MARK PRIORITY  (click / P)",
                infoX + 22, infoY + 285, 13, RAYWHITE);
        }
        else
        {
            DrawText("Select an aircraft on the map to view its model, airline,",
                infoX + 14, infoY + 48, 12, Color{203, 219, 224, 255});
            DrawText("state, route and scheduled times.",
                infoX + 14, infoY + 68, 12, Color{203, 219, 224, 255});
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

        EndDrawing();
    }

    UnloadRenderTexture(airplanePreviewTarget);
    CloseWindow();
    return 0;
}
