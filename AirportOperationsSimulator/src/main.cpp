#include "raylib.h"
#include "camera_controller.h"
#include "airplane.h"

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1024, 768, "Airport Operations Simulator - 3D Airplane Prototype");

    CameraController cameraController;
    Airplane airplane;

    while (!WindowShouldClose())
    {
        cameraController.Update(GetFrameTime());

        BeginDrawing();
        ClearBackground(Color{24, 34, 43, 255});

        BeginMode3D(cameraController.GetCamera3D());
        DrawPlane({0.0f, -3.5f, 0.0f}, {500.0f, 500.0f}, Color{48, 91, 77, 255});
        for (int coordinate = -250; coordinate <= 250; coordinate += 25)
        {
            const Color gridColor = coordinate == 0 ? Color{124, 151, 137, 255} : Color{64, 107, 89, 255};
            DrawLine3D({static_cast<float>(coordinate), -3.4f, -250.0f},
                {static_cast<float>(coordinate), -3.4f, 250.0f}, gridColor);
            DrawLine3D({-250.0f, -3.4f, static_cast<float>(coordinate)},
                {250.0f, -3.4f, static_cast<float>(coordinate)}, gridColor);
        }
        airplane.Draw();

        EndMode3D();

        DrawText("Low-poly 3D airplane prototype", 18, 16, 20, RAYWHITE);
        DrawText("Pan: WASD or arrow keys    Zoom: mouse wheel", 18, 44, 18, Color{185, 201, 207, 255});
        DrawText(
            TextFormat("Camera target: %.0f, %.0f   Zoom: %.2fx",
                cameraController.GetCamera().target.x,
                cameraController.GetCamera().target.y,
                cameraController.GetZoomLevel()),
            18, 69, 16, Color{185, 201, 207, 255});
        DrawText(
            TextFormat("Screen: %d x %d   Render: %d x %d",
                GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight()),
            18, 92, 16, Color{185, 201, 207, 255});

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
