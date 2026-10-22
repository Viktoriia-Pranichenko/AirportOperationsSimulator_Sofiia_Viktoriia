#pragma once

#include "raylib.h"

#include <string>
#include <vector>

enum class AirplaneState
{
    Arriving,
    Landing,
    AwaitingTaxiAuthorization,
    TaxiingToStand,
    Parked,
    PreparingForDeparture,
    TaxiingToRunway,
    AwaitingTakeoffAuthorization,
    TakingOff,
    Departed,
    Holding,
    Delayed,
    Incident
};

enum class AircraftSize { Small, Large };

// Flight data plus a low-poly 3D passenger aircraft. Heading 0 faces along -Z.
// The simulation owns the airplane's state; Draw() only renders that state.
class Airplane
{
public:
    std::string flightId{"AO100"};
    std::string aircraftType{"Passenger jet"};
    AircraftSize size{AircraftSize::Small};
    Color liveryColor{48, 125, 160, 255};
    Vector2 position{0.0f, 0.0f};
    Vector2 targetPosition{0.0f, 0.0f};
    AirplaneState state{AirplaneState::Arriving};
    float headingDegrees{0.0f};
    float speed{0.0f};
    float altitude{0.0f};
    float pitchDegrees{0.0f};
    float delayMinutes{0.0f};
    bool hasTarget{false};
    bool isPriority{false};
    float renderScale{0.325f};
    float turnRateDegreesPerSecond{95.0f};

    // Change state only when the requested transition is valid.
    bool TransitionTo(AirplaneState nextState);

    // Route points are world x/z coordinates. The first point should be the
    // aircraft's current position; optional transitionIndex changes state as
    // the aircraft reaches that waypoint.
    void SetWaypointRoute(std::vector<Vector2> route, float routeSpeed,
        AirplaneState completionState, int transitionIndex = -1,
        AirplaneState intermediateState = AirplaneState::TaxiingToStand,
        int touchdownWaypoint = -1);
    void Update(float deltaTime);

    // Draw at the airplane's current position and heading.
    void Draw(float scale = 1.0f) const;

    // Optional overload for previewing a pose without changing simulation data.
    void Draw(Vector2 drawPosition, float drawHeadingDegrees, float scale) const;

private:
    void DrawPose(Vector2 drawPosition, float drawHeadingDegrees, float scale,
        float drawPitchDegrees, float drawAltitude) const;

    std::vector<Vector2> route;
    std::size_t nextWaypoint{0};
    float routeSpeed{0.0f};
    float landingApproachLength{0.0f};
    std::size_t landingTouchdownWaypoint{0};
    float animationTime{0.0f};
    AirplaneState routeCompletionState{AirplaneState::Parked};
    AirplaneState routeIntermediateState{AirplaneState::TaxiingToStand};
    int transitionWaypointIndex{-1};
    bool intermediateTransitionDone{false};
};
