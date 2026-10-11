#pragma once

#include "raylib.h"

#include <cmath>

class CameraController
{
public:
    CameraController()
        : camera{{0.0f, 0.0f}, {0.0f, 0.0f}, 0.0f, 1.0f}
    {
    }

    void Update(float deltaTime, int screenWidth, int screenHeight)
    {
        camera.offset = {
            static_cast<float>(screenWidth) * 0.5f,
            static_cast<float>(screenHeight) * 0.5f
        };

        float moveX = 0.0f;
        float moveY = 0.0f;

        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) moveX -= 1.0f;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) moveX += 1.0f;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) moveY -= 1.0f;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) moveY += 1.0f;

        if (moveX != 0.0f && moveY != 0.0f)
        {
            moveX *= 0.70710678f;
            moveY *= 0.70710678f;
        }

        const float panSpeed = 500.0f / camera.zoom;
        camera.target.x += moveX * panSpeed * deltaTime;
        camera.target.y += moveY * panSpeed * deltaTime;

        const float wheelMove = GetMouseWheelMove();
        if (wheelMove != 0.0f)
        {
            camera.zoom *= std::pow(1.1f, wheelMove);
            if (camera.zoom < MinZoom) camera.zoom = MinZoom;
            if (camera.zoom > MaxZoom) camera.zoom = MaxZoom;
        }
    }

    const Camera2D& GetCamera() const
    {
        return camera;
    }

private:
    static constexpr float MinZoom = 0.5f;
    static constexpr float MaxZoom = 2.5f;

    Camera2D camera;
};
