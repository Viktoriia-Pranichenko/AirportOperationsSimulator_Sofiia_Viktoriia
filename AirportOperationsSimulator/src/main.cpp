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

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1024, 768, "Airport Operations Simulator");

    CameraController cameraController;
    AirportTerminal terminal;
    AirportGround airportGround(terminal);
    Airplane airplane;
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
    airplane.flightId = "AO100";
    airplane.position = {-60.0f, -507.0f};
    airplane.headingDegrees = 90.0f;

    while (!WindowShouldClose())
    {
        cameraController.Update(GetFrameTime());

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
        airplane.Draw(0.325f);
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
