#include "raylib.h"
#include "camera_controller.h"

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(800, 600, "Airport Operations Simulator - 2D Camera Test");

    CameraController cameraController;

    while (!WindowShouldClose())
    {
        cameraController.Update(GetFrameTime(), GetScreenWidth(), GetScreenHeight());

        BeginDrawing();
        ClearBackground(Color{24, 34, 43, 255});

        BeginMode2D(cameraController.GetCamera());

        for (int coordinate = -2000; coordinate <= 2000; coordinate += 100)
        {
            const Color gridColor = coordinate == 0 ? Color{92, 115, 126, 255} : Color{43, 58, 67, 255};
            DrawLine(coordinate, -2000, coordinate, 2000, gridColor);
            DrawLine(-2000, coordinate, 2000, coordinate, gridColor);
        }

        DrawRectangle(-150, -110, 180, 120, Color{65, 122, 132, 255});
        DrawRectangleLines(-150, -110, 180, 120, Color{174, 220, 206, 255});
        DrawCircle(110, -45, 55.0f, Color{219, 160, 83, 255});
        DrawCircleLines(110, -45, 55.0f, Color{255, 225, 163, 255});
        DrawTriangle(
            Vector2{-150.0f, 90.0f},
            Vector2{-90.0f, -5.0f},
            Vector2{-30.0f, 90.0f},
            Color{183, 111, 112, 255});
        DrawRectangle(50, 70, 120, 90, Color{128, 117, 175, 255});
        DrawRectangleLines(50, 70, 120, 90, Color{207, 194, 245, 255});

        EndMode2D();

        DrawText("2D camera test - temporary geometry", 18, 16, 20, RAYWHITE);
        DrawText("Pan: WASD or arrow keys    Zoom: mouse wheel", 18, 44, 18, Color{185, 201, 207, 255});
        DrawText("Zoom limits: 0.5x - 2.5x", 18, 69, 16, Color{185, 201, 207, 255});
        DrawText(
            TextFormat("Camera target: %.0f, %.0f   Zoom: %.2fx",
                cameraController.GetCamera().target.x,
                cameraController.GetCamera().target.y,
                cameraController.GetCamera().zoom),
            18, 92, 16, Color{185, 201, 207, 255});

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
