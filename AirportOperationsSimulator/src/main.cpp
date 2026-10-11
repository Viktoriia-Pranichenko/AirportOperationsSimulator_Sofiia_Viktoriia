#include "raylib.h"

#include "airport_terminal.h"
#include "airport_ground.h"
#include "airplane.h"
#include "camera_controller.h"

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1024, 768, "Airport Operations Simulator");

    CameraController cameraController;
    AirportTerminal terminal;
    AirportGround airportGround(terminal);
    Airplane airplane;
    airplane.flightId = "AO100";
    airplane.position = {-220.0f, -590.0f};
    airplane.headingDegrees = 90.0f;

    while (!WindowShouldClose())
    {
        cameraController.Update(GetFrameTime());

        BeginDrawing();
        ClearBackground(Color{142, 181, 199, 255});

        BeginMode3D(cameraController.GetCamera3D());
        airportGround.Draw();
        terminal.Draw();
        airplane.Draw(0.65f);
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
