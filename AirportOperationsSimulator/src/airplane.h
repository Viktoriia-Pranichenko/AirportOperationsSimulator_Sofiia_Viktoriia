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

// Flight data plus a low-poly 3D passenger aircraft. Heading 0 faces along -Z.
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
};
