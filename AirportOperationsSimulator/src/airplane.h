#pragma once

#include "raylib.h"

#include <string>

enum class AirplaneState
{
    Arriving,
    Landing,
    TaxiingToStand,
    Parked,
    PreparingForDeparture,
    TaxiingToRunway,
    TakingOff,
    Departed,
    Holding,
    Delayed,
    Incident
};

// A lightweight top-down airplane drawing. The nose points up when heading is 0.
// The simulation owns the airplane's state; Draw() only renders that state.
class Airplane
{
public:
    std::string flightId{"AO100"};
    std::string aircraftType{"Passenger jet"};
    Vector2 position{0.0f, 0.0f};
    Vector2 targetPosition{0.0f, 0.0f};
    AirplaneState state{AirplaneState::Arriving};
    float headingDegrees{0.0f};
    float speed{0.0f};
    float delayMinutes{0.0f};
    bool hasTarget{false};
    bool isPriority{false};

    // Change state only when the requested transition is valid.
    bool TransitionTo(AirplaneState nextState);

    // Draw at the airplane's current position and heading.
    void Draw(float scale = 1.0f) const;

    // Optional overload for previewing a pose without changing simulation data.
    void Draw(Vector2 drawPosition, float drawHeadingDegrees, float scale) const;

private:
    static Vector2 Transform(float x, float y, Vector2 position,
        float cosine, float sine, float scale);
    static void DrawLocalTriangle(Vector2 a, Vector2 b, Vector2 c, Vector2 position,
        float cosine, float sine, float scale, Color color);
    static void DrawLocalCircle(float x, float y, float radius, Vector2 position,
        float cosine, float sine, float scale, Color color);
    static void DrawLocalFan(const Vector2* points, int count, Vector2 position,
        float cosine, float sine, float scale, Color color);

    static constexpr Color WingColor{54, 119, 153, 255};
    static constexpr Color BodyColor{226, 232, 224, 255};
    static constexpr Color EngineColor{43, 61, 70, 255};
    static constexpr Color CockpitColor{77, 174, 196, 255};
    static constexpr Color MarkingColor{224, 153, 67, 255};
};
