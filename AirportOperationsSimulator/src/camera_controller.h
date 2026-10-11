#pragma once

#include "raylib.h"

#include <cmath>

class CameraController
{
public:
    CameraController()
        : camera{
            {static_cast<float>(GetRenderWidth()) * 0.5f, static_cast<float>(GetRenderHeight()) * 0.5f},
            {0.0f, 0.0f},
            0.0f,
            1.0f
        }
    {
    }

    void Update(float deltaTime)
    {
        const int screenWidth = GetScreenWidth();
        const int screenHeight = GetScreenHeight();
        const int renderWidth = GetRenderWidth();
        const int renderHeight = GetRenderHeight();

        camera.offset = {
            static_cast<float>(renderWidth) * 0.5f,
            static_cast<float>(renderHeight) * 0.5f
        };

        const float dpiScale = screenWidth > 0
            ? static_cast<float>(renderWidth) / static_cast<float>(screenWidth)
            : 1.0f;
        camera.zoom = zoomLevel * dpiScale;

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

        const float panSpeed = 500.0f / zoomLevel;
        camera.target.x += moveX * panSpeed * deltaTime;
        camera.target.y += moveY * panSpeed * deltaTime;

        const float wheelMove = GetMouseWheelMove();
        if (wheelMove != 0.0f)
        {
            zoomLevel *= std::pow(1.1f, wheelMove);
            if (zoomLevel < MinZoom) zoomLevel = MinZoom;
            if (zoomLevel > MaxZoom) zoomLevel = MaxZoom;
            camera.zoom = zoomLevel * dpiScale;
        }
    }

    const Camera2D& GetCamera() const
    {
        return camera;
    }

    float GetZoomLevel() const
    {
        return zoomLevel;
    }

private:
    static constexpr float MinZoom = 0.5f;
    static constexpr float MaxZoom = 2.5f;

    Camera2D camera;
    float zoomLevel = 1.0f;
};
