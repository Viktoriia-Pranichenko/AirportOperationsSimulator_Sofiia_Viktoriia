#include "raylib.h"

#include "airport_terminal.h"
#include "airport_ground.h"
#include "airplane.h"
#include "airport_fire_truck.h"
#include "airport_service_vehicles.h"
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
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    struct FlightPlan
    {
        Airplane airplane;
        float arrivalScheduledSeconds{-1.0f};
        float departureScheduledSeconds{-1.0f};
        float preparationTime{0.0f};
        bool hasArrivalSchedule{false};
        bool spawned{false};
        bool departureFlight{false};
        bool arrivalFlight{false};
        bool routeStarted{false};
        bool departureQueued{false};
        bool runwayReserved{false};
        bool takeoffRollStarted{false};
        float takeoffRollTime{0.0f};
        std::uint64_t requestOrder{0};
        std::vector<std::string> activeRouteNodeIds;
        std::string arrivalStartNode;
        std::string parkingStandId;
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
        case AirplaneState::TaxiingToStand: return "TAXI IN";
        case AirplaneState::Parked: return "PARKED";
        case AirplaneState::PreparingForDeparture: return "PREP";
        case AirplaneState::TaxiingToRunway: return "TAXI OUT";
        case AirplaneState::TakingOff: return "TAKEOFF";
        case AirplaneState::Departed: return "DONE";
        default: return "SCHEDULED";
        }
    }
}

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1024, 768, "Airport Operations Simulator");

    CameraController cameraController;
    AirportTerminal terminal;
    AirportGround airportGround(terminal);
    std::vector<FlightPlan> flights;
    BaggageTug baggageTugs[] = {
        {164.55f, -79.3f},
        {169.65f, -79.3f},
        {174.75f, -79.3f},
        {179.85f, -79.3f},
        {184.95f, -79.3f}
    };
    PassengerShuttle passengerShuttles[] = {
        {164.55f, -112.25f},
        {169.65f, -112.25f},
        {174.75f, -112.25f}
    };
    AirportFireTruck airportFireTrucks[] = {
        {179.85f, -112.25f},
        {184.95f, -112.25f}
    };
    AircraftRefuellingTanker refuellingTankers[] = {
        {190.05f, -112.25f},
        {195.15f, -112.25f}
    };
    FollowMeCar followMeCars[] = {
        {190.05f, -39.75f},
        {195.15f, -39.75f}
    };
    AircraftPushbackTug pushbackTugs[] = {
        {200.25f, -39.75f},
        {205.35f, -39.75f},
        {210.45f, -39.75f}
    };
    BaggageBeltLoader baggageBeltLoaders[] = {
        {164.55f, -39.75f},
        {169.65f, -39.75f},
        {174.75f, -39.75f},
        {179.85f, -39.75f},
        {184.95f, -39.75f}
    };
    CateringTruck cateringTrucks[] = {
        {164.55f, -70.25f},
        {169.65f, -70.25f}
    };
    WaterServiceTruck waterServiceTrucks[] = {
        {174.75f, -70.25f},
        {179.85f, -70.25f}
    };
    MaintenancePickupTruck maintenancePickupTrucks[] = {
        {184.95f, -70.25f},
        {190.05f, -70.25f}
    };
    PassengerBoardingStairs passengerBoardingStairs[] = {
        {195.15f, -70.25f},
        {200.25f, -70.25f},
        {205.35f, -70.25f},
        {210.45f, -70.25f},
        {215.55f, -70.25f},
        {220.65f, -70.25f},
        {225.75f, -70.25f},
        {230.85f, -70.25f}
    };
    ServiceBuilding serviceBuilding;
    CarParkingLot carParkingLot;
    // Inbound flights enter on separate runways and use the existing graph to
    // reach different stands. Route nodes also include the curved taxiway
    // samples, so the aircraft can follow the authored bends smoothly.
    struct ArrivalSetup { const char* flight; const char* type; const char* approach;
        const char* runwayId; const char* runwayStart; const char* stand; float runwayZ;
        float arrivalMinute; float departureMinute; };
    const ArrivalSetup arrivals[] = {
        {"AO100", "Passenger jet", "APPROACH_A_W", "RUNWAY_A", "RUNWAY_START_W", "GATE_1", -507.0f, 0.0f, 12.0f},
        {"AO202", "Regional jet", "APPROACH_B_W", "RUNWAY_B", "RUNWAY_B_START_W", "WEST_2", -642.0f, 4.0f, 17.0f},
        {"AO317", "Cargo jet", "APPROACH_C_W", "RUNWAY_C", "RUNWAY_C_START_W", "EAST_3", -777.0f, 8.0f, 22.0f},
        {"AO512", "Passenger jet", "APPROACH_A_W", "RUNWAY_A", "RUNWAY_START_W", "GATE_2", -507.0f, 15.0f, 29.0f}
    };
    for (const ArrivalSetup& setup : arrivals)
    {
        FlightPlan plan;
        plan.airplane.flightId = setup.flight;
        plan.airplane.aircraftType = setup.type;
        const TaxiwayNode* approach = airportGround.FindNode(setup.approach);
        if (approach == nullptr) continue;
        plan.airplane.position = {approach->position.x, approach->position.z};
        plan.airplane.headingDegrees = 90.0f;
        plan.airplane.renderScale = 0.23f;
        plan.airplane.isPriority = std::string(setup.flight) == "AO202";
        plan.hasArrivalSchedule = true;
        plan.arrivalScheduledSeconds = setup.arrivalMinute * 60.0f;
        plan.departureScheduledSeconds = setup.departureMinute * 60.0f;
        plan.arrivalStartNode = setup.approach;
        plan.parkingStandId = setup.stand;
        plan.runwayId = setup.runwayId;
        plan.departureRunwayStart = setup.runwayStart;
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
        plan.airplane.position = {stand->position.x, stand->position.z};
        plan.airplane.headingDegrees = 180.0f;
        plan.airplane.state = AirplaneState::Parked;
        plan.airplane.renderScale = 0.23f;
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
        flights.push_back(std::move(plan));
    }

    std::deque<std::size_t> arrivalQueue;
    std::deque<std::size_t> departureQueue;
    std::unordered_map<std::string, std::string> occupiedNodes;
    std::unordered_map<std::string, std::string> occupiedSections;
    std::unordered_map<std::string, std::string> occupiedStands;
    std::unordered_map<std::string, std::string> runwayOwners;
    std::uint64_t nextRequestOrder = 0;
    float shiftClockSeconds = 0.0f;
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

    while (!WindowShouldClose())
    {
        const float deltaTime = GetFrameTime();
        shiftClockSeconds += deltaTime * ShiftClockRate;
        cameraController.Update(deltaTime);
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
                    airplane.position = {approach->position.x, approach->position.z};
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
                    occupiedStands[plan.parkingStandId] = airplane.flightId;
                    occupiedNodes[plan.parkingStandId] = airplane.flightId;
                }
            }
            if (!plan.spawned) continue;
            airplane.Update(deltaTime);
            if (airplane.state == AirplaneState::Parked)
                airplane.headingDegrees = plan.parkingHeadingDegrees;

            if (plan.arrivalFlight && plan.routeStarted &&
                airplane.state == AirplaneState::TaxiingToStand && plan.runwayReserved)
            {
                const auto owner = runwayOwners.find(plan.runwayId);
                if (owner != runwayOwners.end() && owner->second == airplane.flightId)
                    runwayOwners.erase(owner);
                plan.runwayReserved = false;
            }
            if (plan.arrivalFlight && plan.routeStarted && airplane.state == AirplaneState::Parked)
            {
                releaseRoute(plan.activeRouteNodeIds, airplane.flightId);
                plan.activeRouteNodeIds.clear();
                plan.routeStarted = false;
                plan.arrivalFlight = false;
                occupiedNodes[plan.parkingStandId] = airplane.flightId;
            }
            if (!plan.departureFlight || airplane.state == AirplaneState::Departed) continue;

            // Departures end at the runway for now. The aircraft does not
            // continue into an airborne animation; that can be added later.
            if (airplane.state == AirplaneState::TakingOff && !airplane.hasTarget)
            {
                if (!plan.takeoffRollStarted)
                {
                    plan.takeoffRollStarted = true;
                    plan.takeoffRollTime = 1.25f;
                }
                else
                {
                    plan.takeoffRollTime -= deltaTime;
                    if (plan.takeoffRollTime <= 0.0f)
                    {
                        airplane.TransitionTo(AirplaneState::Departed);
                        releaseRoute(plan.activeRouteNodeIds, airplane.flightId);
                        plan.activeRouteNodeIds.clear();
                        const auto runwayOwner = runwayOwners.find(plan.runwayId);
                        if (plan.runwayReserved && runwayOwner != runwayOwners.end() &&
                            runwayOwner->second == airplane.flightId)
                        {
                            runwayOwners.erase(plan.runwayId);
                            plan.runwayReserved = false;
                        }
                        const auto standOwner = occupiedStands.find(plan.parkingStandId);
                        if (standOwner != occupiedStands.end() && standOwner->second == airplane.flightId)
                            occupiedStands.erase(standOwner);
                        const auto standNodeOwner = occupiedNodes.find(plan.parkingStandId);
                        if (standNodeOwner != occupiedNodes.end() &&
                            standNodeOwner->second == airplane.flightId)
                            occupiedNodes.erase(standNodeOwner);
                    }
                }
                continue;
            }

            if (airplane.state == AirplaneState::Parked && !airplane.hasTarget)
            {
                if (shiftClockSeconds >= plan.departureScheduledSeconds)
                {
                    airplane.TransitionTo(AirplaneState::PreparingForDeparture);
                    plan.preparationTime = 3.0f;
                }
            }
            else if (airplane.state == AirplaneState::PreparingForDeparture)
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
        while (!arrivalQueue.empty() || !departureQueue.empty())
        {
            struct Request { std::uint64_t order; bool arrival; std::size_t flightIndex; };
            std::vector<Request> requests;
            requests.reserve(arrivalQueue.size() + departureQueue.size());
            for (const std::size_t index : arrivalQueue)
                requests.push_back({flights[index].requestOrder, true, index});
            for (const std::size_t index : departureQueue)
                requests.push_back({flights[index].requestOrder, false, index});
            std::stable_sort(requests.begin(), requests.end(),
                [](const Request& left, const Request& right) { return left.order < right.order; });

            bool foundSafeRequest = false;
            bool serveArrival = false;
            std::size_t flightIndex = 0;
            std::vector<std::string> routeNodeIds;
            for (const Request& request : requests)
            {
                FlightPlan& candidate = flights[request.flightIndex];
                const std::string& start = request.arrival ? candidate.arrivalStartNode :
                    candidate.parkingStandId;
                const std::string& destination = request.arrival ? candidate.parkingStandId :
                    candidate.departureRunwayStart;
                const auto selectedRunway = std::find_if(airportGround.GetRunways().begin(),
                    airportGround.GetRunways().end(),
                    [&candidate](const AirportRunway& runway)
                    { return runway.id == candidate.runwayId; });
                if (selectedRunway == airportGround.GetRunways().end() ||
                    !selectedRunway->available)
                    continue;
                if (runwayOwners.find(candidate.runwayId) != runwayOwners.end())
                    continue;
                const auto standOwner = occupiedStands.find(destination);
                if (request.arrival && standOwner != occupiedStands.end() &&
                    standOwner->second != candidate.airplane.flightId)
                    continue;

                std::vector<std::string> candidateRoute =
                    airportGround.FindRouteNodeIds(start, destination);
                if (candidateRoute.empty() ||
                    !routeCanBeReserved(candidateRoute, candidate.airplane.flightId))
                    continue;
                foundSafeRequest = true;
                serveArrival = request.arrival;
                flightIndex = request.flightIndex;
                routeNodeIds = std::move(candidateRoute);
                break;
            }
            if (!foundSafeRequest) break;

            FlightPlan& plan = flights[flightIndex];
            auto& requestQueue = serveArrival ? arrivalQueue : departureQueue;
            const auto queuedRequest = std::find(requestQueue.begin(), requestQueue.end(), flightIndex);
            if (queuedRequest == requestQueue.end()) break;
            requestQueue.erase(queuedRequest);

            reserveRoute(routeNodeIds, plan.airplane.flightId);
            plan.activeRouteNodeIds = routeNodeIds;
            plan.routeStarted = true;
            plan.runwayReserved = true;
            runwayOwners[plan.runwayId] = plan.airplane.flightId;
            if (serveArrival)
            {
                plan.airplane.TransitionTo(AirplaneState::Arriving);
                plan.airplane.TransitionTo(AirplaneState::Landing);
                const std::vector<Vector2> route = MakeWorldRoute(airportGround, routeNodeIds);
                const int exitWaypoint = FindRunwayExitWaypoint(
                    airportGround, routeNodeIds, plan.runwayZ);
                plan.airplane.SetWaypointRoute(route, 19.0f, AirplaneState::Parked,
                    exitWaypoint, AirplaneState::TaxiingToStand);
            }
            else
            {
                plan.departureQueued = false;
                if (plan.airplane.TransitionTo(AirplaneState::TaxiingToRunway))
                {
                    const std::vector<Vector2> route = MakeWorldRoute(airportGround, routeNodeIds);
                    plan.airplane.SetWaypointRoute(route, 11.0f, AirplaneState::TakingOff);
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
        serviceBuilding.Draw();
        for (const BaggageTug& baggageTug : baggageTugs)
            baggageTug.Draw();
        for (const PassengerShuttle& shuttle : passengerShuttles)
            shuttle.Draw();
        for (const AirportFireTruck& fireTruck : airportFireTrucks)
            fireTruck.Draw();
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

        DrawText("Pan: WASD or arrow keys    Zoom: mouse wheel", 18, 16, 18, RAYWHITE);
        DrawText(
            TextFormat("Zoom: %.2fx   Render: %d x %d",
                cameraController.GetZoomLevel(), GetRenderWidth(), GetRenderHeight()),
            18, 42, 16, Color{185, 201, 207, 255});

        DrawText(TextFormat("SHIFT %s", FormatShiftTime(shiftClockSeconds).c_str()),
            GetRenderWidth() - 190, 16, 18, RAYWHITE);
        const int panelX = 16;
        const int panelY = 74;
        const int rowHeight = 21;
        DrawRectangle(panelX, panelY, 390, 35 + static_cast<int>(flights.size()) * rowHeight,
            Fade(Color{13, 26, 34, 255}, 0.88f));
        DrawRectangle(panelX, panelY, 390, 28, Color{33, 66, 82, 255});
        DrawText("FLIGHT SCHEDULE   ID      ARR     DEP     STATUS",
            panelX + 9, panelY + 7, 13, RAYWHITE);
        for (std::size_t i = 0; i < flights.size(); ++i)
        {
            const FlightPlan& plan = flights[i];
            const std::string arr = plan.hasArrivalSchedule ?
                FormatShiftTime(plan.arrivalScheduledSeconds) : "--:--";
            const std::string dep = plan.departureFlight ?
                FormatShiftTime(plan.departureScheduledSeconds) : "--:--";
            const char* status = plan.spawned ? FlightStateLabel(plan.airplane.state) : "SCHEDULED";
            const Color rowColor = plan.spawned && plan.airplane.state != AirplaneState::Departed ?
                Color{235, 242, 245, 255} : Color{166, 184, 191, 255};
            DrawText(TextFormat("%s      %s   %s   %s", plan.airplane.flightId.c_str(),
                arr.c_str(), dep.c_str(), status), panelX + 12,
                panelY + 34 + static_cast<int>(i) * rowHeight, 14, rowColor);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
