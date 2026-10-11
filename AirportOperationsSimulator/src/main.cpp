#include "raylib.h"
#include "camera_controller.h"
#include "airplane.h"

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1024, 768, "Airport Operations Simulator - 2D Camera Test");

    CameraController cameraController;
    Airplane airplane;

    while (!WindowShouldClose())
    {
        cameraController.Update(GetFrameTime());

        BeginDrawing();
        ClearBackground(Color{24, 34, 43, 255});

        BeginMode2D(cameraController.GetCamera());

        for (int coordinate = -2000; coordinate <= 2000; coordinate += 100)
        {
            const Color gridColor = coordinate == 0 ? Color{92, 115, 126, 255} : Color{43, 58, 67, 255};
            DrawLine(coordinate, -2000, coordinate, 2000, gridColor);
            DrawLine(-2000, coordinate, 2000, coordinate, gridColor);
        }

        airplane.Draw();

        EndMode2D();

        DrawText("Airplane prototype - top-down airport view", 18, 16, 20, RAYWHITE);
        DrawText("Pan: WASD or arrow keys    Zoom: mouse wheel", 18, 44, 18, Color{185, 201, 207, 255});
        DrawText("Zoom limits: 0.5x - 2.5x", 18, 69, 16, Color{185, 201, 207, 255});
        DrawText(
            TextFormat("Camera target: %.0f, %.0f   Zoom: %.2fx",
                cameraController.GetCamera().target.x,
                cameraController.GetCamera().target.y,
                cameraController.GetZoomLevel()),
            18, 92, 16, Color{185, 201, 207, 255});
        DrawText(
            TextFormat("Screen: %d x %d   Render: %d x %d",
                GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight()),
            18, 115, 16, Color{185, 201, 207, 255});

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
