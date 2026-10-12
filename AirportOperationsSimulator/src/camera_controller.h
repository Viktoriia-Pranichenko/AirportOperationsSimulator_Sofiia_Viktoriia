#pragma once

#include "raylib.h"

#include <cmath>

class CameraController
{
public:
    CameraController()
        : camera2D{
            {static_cast<float>(GetRenderWidth()) * 0.5f, static_cast<float>(GetRenderHeight()) * 0.5f},
            {0.0f, 0.0f},
            0.0f,
            1.0f
        },
        camera3D{
            {0.0f, 132.0f, -230.0f},
            {0.0f, 6.0f, -45.0f},
            {0.0f, 1.0f, 0.0f},
            42.0f,
            CAMERA_PERSPECTIVE
        }
    {
    }

    void Update(float deltaTime)
    {
        const int screenWidth = GetScreenWidth();
        const int renderWidth = GetRenderWidth();
        const int renderHeight = GetRenderHeight();
        camera2D.offset = {
            static_cast<float>(renderWidth) * 0.5f,
            static_cast<float>(renderHeight) * 0.5f
        };
        const float dpiScale = screenWidth > 0
            ? static_cast<float>(renderWidth) / static_cast<float>(screenWidth)
            : 1.0f;
        camera2D.zoom = zoomLevel * dpiScale;
        if (gateViewActive) return;

        float moveRight = 0.0f;
        float moveForward = 0.0f;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) moveRight -= 1.0f;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) moveRight += 1.0f;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) moveForward += 1.0f;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) moveForward -= 1.0f;

        if (moveRight != 0.0f && moveForward != 0.0f)
        {
            moveRight *= 0.70710678f;
            moveForward *= 0.70710678f;
        }

        float forwardX = camera3D.target.x - camera3D.position.x;
        float forwardZ = camera3D.target.z - camera3D.position.z;
        const float forwardLength = std::sqrt(forwardX * forwardX + forwardZ * forwardZ);
        if (forwardLength > 0.0f)
        {
            forwardX /= forwardLength;
            forwardZ /= forwardLength;
            const float rightX = -forwardZ;
            const float rightZ = forwardX;
            const float panSpeed = 30.0f / zoomLevel;
            const Vector3 shift{
                (rightX * moveRight + forwardX * moveForward) * panSpeed * deltaTime,
                0.0f,
                (rightZ * moveRight + forwardZ * moveForward) * panSpeed * deltaTime
            };
            camera3D.target.x += shift.x;
            camera3D.target.z += shift.z;
            camera3D.position.x += shift.x;
            camera3D.position.z += shift.z;
        }

        // Keep the screen-space camera state available for the existing 2D UI layer.
        const float uiPanSpeed = 575.0f / zoomLevel * deltaTime;
        camera2D.target.x -= moveRight * uiPanSpeed;
        camera2D.target.y -= moveForward * uiPanSpeed;

        const float wheelMove = GetMouseWheelMove();
        if (wheelMove != 0.0f)
        {
            zoomLevel *= std::pow(1.1f, wheelMove);
            if (zoomLevel < MinZoom) zoomLevel = MinZoom;
            if (zoomLevel > MaxZoom) zoomLevel = MaxZoom;
            camera2D.zoom = zoomLevel * dpiScale;

            const float dx = camera3D.position.x - camera3D.target.x;
            const float dy = camera3D.position.y - camera3D.target.y;
            const float dz = camera3D.position.z - camera3D.target.z;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float desiredDistance = InitialDistance / zoomLevel;
            if (distance > 0.0f)
            {
                const float ratio = desiredDistance / distance;
                camera3D.position = {
                    camera3D.target.x + dx * ratio,
                    camera3D.target.y + dy * ratio,
                    camera3D.target.z + dz * ratio
                };
            }
        }
    }

    const Camera2D& GetCamera() const { return camera2D; }
    const Camera3D& GetCamera3D() const { return camera3D; }
    float GetZoomLevel() const { return zoomLevel; }
    bool IsGateViewActive() const { return gateViewActive; }

    void EnterGateView(Vector3 focus, float gateOrientationDegrees)
    {
        if (!gateViewActive)
        {
            overviewCamera = camera3D;
            overviewZoomLevel = zoomLevel;
        }
        gateViewActive = true;
        constexpr float CameraHeight = 88.0f;
        const float headingRadians = gateOrientationDegrees * DEG2RAD;
        camera3D = {
            {focus.x, focus.y + CameraHeight, focus.z},
            focus,
            {std::sin(headingRadians), 0.0f, -std::cos(headingRadians)},
            48.0f,
            CAMERA_PERSPECTIVE
        };
    }

    void ReturnToOverview()
    {
        if (!gateViewActive) return;
        camera3D = overviewCamera;
        zoomLevel = overviewZoomLevel;
        gateViewActive = false;
    }

private:
    static constexpr float MinZoom = 0.5f;
    static constexpr float MaxZoom = 2.5f;
    static constexpr float InitialDistance = 223.8f;

    Camera2D camera2D;
    Camera3D camera3D;
    Camera3D overviewCamera{};
    float zoomLevel = 1.0f;
    float overviewZoomLevel = 1.0f;
    bool gateViewActive = false;
};
