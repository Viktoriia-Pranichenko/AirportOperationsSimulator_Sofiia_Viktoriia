#include "airplane.h"

#include <cmath>

bool Airplane::TransitionTo(AirplaneState nextState)
{
    bool allowed = false;

    switch (state)
    {
    case AirplaneState::Arriving:
        allowed = nextState == AirplaneState::Landing || nextState == AirplaneState::Holding;
        break;
    case AirplaneState::Holding:
        allowed = nextState == AirplaneState::Arriving || nextState == AirplaneState::Landing;
        break;
    case AirplaneState::Landing:
        allowed = nextState == AirplaneState::TaxiingToStand;
        break;
    case AirplaneState::TaxiingToStand:
        allowed = nextState == AirplaneState::Parked;
        break;
    case AirplaneState::Parked:
        allowed = nextState == AirplaneState::PreparingForDeparture;
        break;
    case AirplaneState::PreparingForDeparture:
        allowed = nextState == AirplaneState::TaxiingToRunway || nextState == AirplaneState::Delayed;
        break;
    case AirplaneState::Delayed:
        allowed = nextState == AirplaneState::PreparingForDeparture;
        break;
    case AirplaneState::TaxiingToRunway:
        allowed = nextState == AirplaneState::TakingOff || nextState == AirplaneState::Delayed;
        break;
    case AirplaneState::TakingOff:
        allowed = nextState == AirplaneState::Departed;
        break;
    case AirplaneState::Departed:
    case AirplaneState::Incident:
        allowed = false;
        break;
    }

    // Incidents can interrupt any active operation, but not a completed flight.
    if (nextState == AirplaneState::Incident && state != AirplaneState::Departed)
        allowed = true;

    if (!allowed) return false;
    state = nextState;
    return true;
}

void Airplane::Draw(float scale) const
{
    Draw(position, headingDegrees, scale);
}

void Airplane::Draw(Vector2 drawPosition, float drawHeadingDegrees, float scale) const
{
    const float radians = drawHeadingDegrees * DEG2RAD;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    // Broad, swept wings are the main visual cue that this is an airliner.
    DrawLocalTriangle({-5, -10}, {-48, 13}, {-43, 21}, drawPosition, cosine, sine, scale, WingColor);
    DrawLocalTriangle({-5, -10}, {-43, 21}, {-7, 13}, drawPosition, cosine, sine, scale, WingColor);
    DrawLocalTriangle({5, -10}, {48, 13}, {43, 21}, drawPosition, cosine, sine, scale, WingColor);
    DrawLocalTriangle({5, -10}, {43, 21}, {7, 13}, drawPosition, cosine, sine, scale, WingColor);

    // Tailplane and vertical tail, behind the body.
    DrawLocalTriangle({-3, 26}, {-22, 35}, {-19, 39}, drawPosition, cosine, sine, scale, WingColor);
    DrawLocalTriangle({-3, 26}, {-19, 39}, {0, 34}, drawPosition, cosine, sine, scale, WingColor);
    DrawLocalTriangle({3, 26}, {22, 35}, {19, 39}, drawPosition, cosine, sine, scale, WingColor);
    DrawLocalTriangle({3, 26}, {19, 39}, {0, 34}, drawPosition, cosine, sine, scale, WingColor);

    // Two dark engine nacelles hang from the wings.
    DrawLocalCircle(-24, 12, 5.0f, drawPosition, cosine, sine, scale, EngineColor);
    DrawLocalCircle(24, 12, 5.0f, drawPosition, cosine, sine, scale, EngineColor);

    // Fuselage: a broad center tapers to a recognizable pointed nose.
    const Vector2 fuselage[] = {
        {0, -46}, {-2.5f, -41}, {-4, -25}, {-5, 5}, {-4.5f, 27},
        {-3, 36}, {0, 40}, {3, 36}, {4.5f, 27}, {5, 5},
        {4, -25}, {2.5f, -41}
    };
    DrawLocalFan(fuselage, 12, drawPosition, cosine, sine, scale, BodyColor);

    // Cockpit and a simple cabin stripe add detail at small zoom levels.
    DrawLocalTriangle({0, -39}, {-2.5f, -31}, {2.5f, -31}, drawPosition,
        cosine, sine, scale, CockpitColor);
    // A short row of cabin windows makes the passenger aircraft role clearer.
    DrawLocalCircle(-2.0f, -20, 0.8f, drawPosition, cosine, sine, scale, CockpitColor);
    DrawLocalCircle(2.0f, -20, 0.8f, drawPosition, cosine, sine, scale, CockpitColor);
    DrawLocalCircle(-2.0f, -14, 0.8f, drawPosition, cosine, sine, scale, CockpitColor);
    DrawLocalCircle(2.0f, -14, 0.8f, drawPosition, cosine, sine, scale, CockpitColor);
    DrawLocalCircle(-2.0f, -8, 0.8f, drawPosition, cosine, sine, scale, CockpitColor);
    DrawLocalCircle(2.0f, -8, 0.8f, drawPosition, cosine, sine, scale, CockpitColor);
    DrawLocalCircle(-2.0f, -2, 0.8f, drawPosition, cosine, sine, scale, CockpitColor);
    DrawLocalCircle(2.0f, -2, 0.8f, drawPosition, cosine, sine, scale, CockpitColor);
    DrawLocalCircle(-2.0f, 4, 0.8f, drawPosition, cosine, sine, scale, CockpitColor);
    DrawLocalCircle(2.0f, 4, 0.8f, drawPosition, cosine, sine, scale, CockpitColor);
    DrawLocalCircle(-2.0f, 10, 0.8f, drawPosition, cosine, sine, scale, CockpitColor);
    DrawLocalCircle(2.0f, 10, 0.8f, drawPosition, cosine, sine, scale, CockpitColor);
    DrawLocalTriangle({-3.5f, 18}, {3.5f, 18}, {3.5f, 23}, drawPosition,
        cosine, sine, scale, MarkingColor);
    DrawLocalTriangle({-3.5f, 18}, {3.5f, 23}, {-3.5f, 23}, drawPosition,
        cosine, sine, scale, MarkingColor);
}

Vector2 Airplane::Transform(float x, float y, Vector2 position,
    float cosine, float sine, float scale)
{
    return {
        position.x + (x * cosine - y * sine) * scale,
        position.y + (x * sine + y * cosine) * scale
    };
}

void Airplane::DrawLocalTriangle(Vector2 a, Vector2 b, Vector2 c, Vector2 position,
    float cosine, float sine, float scale, Color color)
{
    // Keep the same winding for every triangle. Mirrored right-side geometry
    // otherwise has the opposite winding and can be culled by the renderer.
    const float winding = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (winding > 0.0f)
    {
        const Vector2 temporary = b;
        b = c;
        c = temporary;
    }

    DrawTriangle(Transform(a.x, a.y, position, cosine, sine, scale),
        Transform(b.x, b.y, position, cosine, sine, scale),
        Transform(c.x, c.y, position, cosine, sine, scale), color);
}

void Airplane::DrawLocalCircle(float x, float y, float radius, Vector2 position,
    float cosine, float sine, float scale, Color color)
{
    DrawCircleV(Transform(x, y, position, cosine, sine, scale), radius * scale, color);
}

void Airplane::DrawLocalFan(const Vector2* points, int count, Vector2 position,
    float cosine, float sine, float scale, Color color)
{
    // Fill from the fuselage center with individually wound triangles. This
    // avoids gaps from a triangle fan when the renderer uses face culling.
    const Vector2 center{0.0f, 0.0f};
    for (int i = 0; i < count; ++i)
    {
        const int next = (i + 1) % count;
        DrawLocalTriangle(center, points[i], points[next], position,
            cosine, sine, scale, color);
    }
}
