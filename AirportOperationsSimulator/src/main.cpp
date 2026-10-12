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
#include <string>
#include <utility>
#include <vector>

namespace
{
    struct FlightPlan
    {
        Airplane airplane;
        float departureDelay{0.0f};
        float preparationTime{0.0f};
        bool departureFlight{false};
        bool takeoffRollStarted{false};
        float takeoffRollTime{0.0f};
        std::string parkingStandId;
        std::string departureRunwayStart{"RUNWAY_START_W"};
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
        for (std::size_t i = 1; i < route.size(); ++i)
        {
            const TaxiwayNode* node = ground.FindNode(route[i]);
            if (node != nullptr && std::abs(node->position.z - runwayZ) > 1.0f)
                return static_cast<int>(i);
        }
        return -1;
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
    struct ArrivalSetup { const char* flight; const char* type; const char* runwayStart;
        const char* stand; float runwayZ; };
    const ArrivalSetup arrivals[] = {
        {"AO100", "Passenger jet", "RUNWAY_START_W", "GATE_1", -507.0f},
        {"AO202", "Regional jet", "RUNWAY_B_START_W", "WEST_2", -642.0f},
        {"AO317", "Cargo jet", "RUNWAY_C_START_W", "EAST_3", -777.0f}
    };
    for (const ArrivalSetup& setup : arrivals)
    {
        const std::vector<std::string> nodeIds = airportGround.FindRouteNodeIds(
            setup.runwayStart, setup.stand);
        std::vector<Vector2> route = MakeWorldRoute(airportGround, nodeIds);
        if (route.empty()) continue;

        FlightPlan plan;
        plan.airplane.flightId = setup.flight;
        plan.airplane.aircraftType = setup.type;
        plan.airplane.position = route.front();
        plan.airplane.headingDegrees = 90.0f;
        plan.airplane.renderScale = 0.23f;
        plan.airplane.isPriority = std::string(setup.flight) == "AO202";
        plan.parkingStandId = setup.stand;
        plan.departureRunwayStart = setup.runwayStart;
        plan.departureFlight = true;
        plan.departureDelay = 12.0f + static_cast<float>(flights.size()) * 7.0f;
        for (const AirportParkingStand& stand : terminal.GetParkingStands())
        {
            if (stand.id == plan.parkingStandId)
            {
                plan.parkingHeadingDegrees = stand.headingDegrees;
                break;
            }
        }
        plan.airplane.TransitionTo(AirplaneState::Landing);
        const int exitWaypoint = FindRunwayExitWaypoint(airportGround, nodeIds, setup.runwayZ);
        plan.airplane.SetWaypointRoute(std::move(route), 19.0f,
            AirplaneState::Parked, exitWaypoint, AirplaneState::TaxiingToStand);
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
        plan.departureDelay = 5.0f;
        plan.parkingStandId = "GATE_4";
        flights.push_back(std::move(plan));
    }

    while (!WindowShouldClose())
    {
        const float deltaTime = GetFrameTime();
        cameraController.Update(deltaTime);
        for (FlightPlan& plan : flights)
        {
            Airplane& airplane = plan.airplane;
            airplane.Update(deltaTime);
            if (airplane.state == AirplaneState::Parked)
                airplane.headingDegrees = plan.parkingHeadingDegrees;
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
                        airplane.TransitionTo(AirplaneState::Departed);
                }
                continue;
            }

            if (airplane.state == AirplaneState::Parked && !airplane.hasTarget)
            {
                plan.departureDelay -= deltaTime;
                if (plan.departureDelay <= 0.0f)
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
                    const auto taxiNodes = airportGround.FindRouteNodeIds(
                        plan.parkingStandId, plan.departureRunwayStart);
                    std::vector<Vector2> taxiRoute = MakeWorldRoute(airportGround, taxiNodes);
                    if (!taxiRoute.empty() && airplane.TransitionTo(AirplaneState::TaxiingToRunway))
                        airplane.SetWaypointRoute(std::move(taxiRoute), 11.0f, AirplaneState::TakingOff);
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
            if (plan.airplane.state != AirplaneState::Departed)
                plan.airplane.Draw(plan.airplane.renderScale);
        EndMode3D();

        DrawText("Pan: WASD or arrow keys    Zoom: mouse wheel", 18, 16, 18, RAYWHITE);
        DrawText(
            TextFormat("Zoom: %.2fx   Render: %d x %d",
                cameraController.GetZoomLevel(), GetRenderWidth(), GetRenderHeight()),
            18, 42, 16, Color{185, 201, 207, 255});

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
