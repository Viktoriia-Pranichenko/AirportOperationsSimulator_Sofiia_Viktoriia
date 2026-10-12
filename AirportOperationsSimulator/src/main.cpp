#include "raylib.h"

#include "airport_terminal.h"
#include "airport_ground.h"
#include "airplane.h"
#include "camera_controller.h"
#include "car_parking_lot.h"
#include "service_building.h"

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1024, 768, "Airport Operations Simulator");

    CameraController cameraController;
    AirportTerminal terminal;
    AirportGround airportGround(terminal);
    Airplane airplane;
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
