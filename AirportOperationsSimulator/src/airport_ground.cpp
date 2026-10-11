#include "airport_ground.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
    constexpr Color Grass{78, 112, 91, 255};
    constexpr Color RunwayShoulder{79, 86, 89, 255};
    constexpr Color RunwaySurface{48, 55, 60, 255};
    constexpr Color TaxiwayShoulder{86, 98, 96, 255};
    constexpr Color TaxiwaySurface{112, 119, 112, 255};
    constexpr Color TaxiwayCenterline{241, 195, 70, 255};
    constexpr Color RunwayMarking{235, 236, 226, 255};
    constexpr Color IntersectionMarking{255, 220, 105, 255};
    constexpr float RunwayCenterZ = -590.0f;
    constexpr float RunwayLength = 880.0f;
    constexpr float RunwayWidth = 92.0f;

    void DrawRunway()
    {
        DrawCubeV({0.0f, -0.03f, RunwayCenterZ},
            {RunwayLength + 28.0f, 0.12f, RunwayWidth + 28.0f}, RunwayShoulder);
        DrawCubeV({0.0f, 0.05f, RunwayCenterZ},
            {RunwayLength, 0.12f, RunwayWidth}, RunwaySurface);

        const float edgeX = RunwayLength * 0.5f - 18.0f;
        const float northEdge = RunwayCenterZ - RunwayWidth * 0.5f + 9.0f;
        const float southEdge = RunwayCenterZ + RunwayWidth * 0.5f - 9.0f;
        DrawLine3D({-edgeX, 0.12f, northEdge}, {edgeX, 0.12f, northEdge}, RunwayMarking);
        DrawLine3D({-edgeX, 0.12f, southEdge}, {edgeX, 0.12f, southEdge}, RunwayMarking);

        for (float x = -420.0f; x <= 420.0f; x += 72.0f)
            DrawLine3D({x, 0.13f, RunwayCenterZ}, {x + 34.0f, 0.13f, RunwayCenterZ}, RunwayMarking);

        for (int bar = 0; bar < 6; ++bar)
        {
            const float leftX = -RunwayLength * 0.5f + 20.0f + bar * 7.0f;
            const float rightX = RunwayLength * 0.5f - 24.0f - bar * 7.0f;
            for (int side = -1; side <= 1; side += 2)
            {
                const float z = RunwayCenterZ + side * (14.0f + bar * 4.0f);
                DrawLine3D({leftX, 0.13f, z}, {leftX, 0.13f, z + side * 12.0f}, RunwayMarking);
                DrawLine3D({rightX, 0.13f, z}, {rightX, 0.13f, z + side * 12.0f}, RunwayMarking);
            }
        }

        // Threshold and displaced-threshold bars at both runway ends.
        for (int i = 0; i < 8; ++i)
        {
            const float z = RunwayCenterZ - 25.0f + i * 7.0f;
            const float leftStart = -RunwayLength * 0.5f + 24.0f + (i % 2) * 4.0f;
            const float rightStart = RunwayLength * 0.5f - 32.0f - (i % 2) * 4.0f;
            DrawLine3D({leftStart, 0.13f, z}, {leftStart + 23.0f, 0.13f, z}, RunwayMarking);
            DrawLine3D({rightStart - 23.0f, 0.13f, z}, {rightStart, 0.13f, z}, RunwayMarking);
        }
    }
}

AirportGround::AirportGround(const AirportTerminal& terminal)
{
    // Parallel taxiway, with three runway exits aligned to the runway edge.
    AddNode("A_WEST", {-450.0f, 0.08f, -465.0f});
    AddNode("A_EXIT_W", {-400.0f, 0.08f, -465.0f});
    AddNode("A_CENTER", {0.0f, 0.08f, -465.0f});
    AddNode("A_EXIT_E", {400.0f, 0.08f, -465.0f});
    AddNode("A_EAST", {450.0f, 0.08f, -465.0f});
    Connect("A_WEST", "A_EXIT_W", 22.0f);
    Connect("A_EXIT_W", "A_CENTER", 22.0f);
    Connect("A_CENTER", "A_EXIT_E", 22.0f);
    Connect("A_EXIT_E", "A_EAST", 22.0f);

    // Runway exits cross the shoulder and join Taxiway A at right angles.
    AddNode("RWY_EXIT_W", {-400.0f, 0.08f, -540.0f});
    AddNode("RWY_EXIT_C", {0.0f, 0.08f, -540.0f});
    AddNode("RWY_EXIT_E", {400.0f, 0.08f, -540.0f});
    Connect("RWY_EXIT_W", "A_EXIT_W", 18.0f);
    Connect("RWY_EXIT_C", "A_CENTER", 18.0f);
    Connect("RWY_EXIT_E", "A_EXIT_E", 18.0f);

    // Explicit runway graph nodes let route planning continue across the runway.
    AddNode("RUNWAY_W", {-400.0f, 0.08f, -590.0f});
    AddNode("RUNWAY_CENTER", {0.0f, 0.08f, -590.0f});
    AddNode("RUNWAY_E", {400.0f, 0.08f, -590.0f});
    Connect("RUNWAY_W", "RUNWAY_CENTER", 1.0f, false);
    Connect("RUNWAY_CENTER", "RUNWAY_E", 1.0f, false);
    Connect("RUNWAY_W", "RWY_EXIT_W", 1.0f, false);
    Connect("RUNWAY_CENTER", "RWY_EXIT_C", 1.0f, false);
    Connect("RUNWAY_E", "RWY_EXIT_E", 1.0f, false);

    // The main apron connector ends before the terminal's marked stand area.
    AddNode("APRON_ENTRY", {0.0f, 0.08f, -220.0f});
    AddNode("APRON_NORTH", {0.0f, 0.08f, -205.0f});
    AddNode("APRON_MID", {0.0f, 0.08f, -180.0f});
    AddNode("APRON_WING_NORTH", {0.0f, 0.08f, -140.0f});
    AddNode("APRON_WING_MIDDLE", {0.0f, 0.08f, -100.0f});
    AddNode("APRON_WING_SOUTH", {0.0f, 0.08f, -60.0f});
    AddNode("APRON_BACK_ROW", {0.0f, 0.08f, -45.0f});
    Connect("A_CENTER", "APRON_ENTRY", 20.0f);
    Connect("APRON_ENTRY", "APRON_NORTH", 16.0f);
    Connect("APRON_NORTH", "APRON_MID", 12.0f);
    Connect("APRON_MID", "APRON_WING_NORTH", 10.0f);
    Connect("APRON_WING_NORTH", "APRON_WING_MIDDLE", 10.0f);
    Connect("APRON_WING_MIDDLE", "APRON_WING_SOUTH", 10.0f);
    Connect("APRON_WING_SOUTH", "APRON_BACK_ROW", 10.0f);

    // Connect the six side stands to the center apron lane at their lead-in points.
    const std::vector<AirportParkingStand>& stands = terminal.GetParkingStands();
    for (const AirportParkingStand& stand : stands)
    {
        AddNode(stand.id, {stand.position.x, 0.08f, stand.position.z});
        if (stand.id.rfind("WEST_", 0) == 0 || stand.id.rfind("EAST_", 0) == 0)
        {
            const std::string approachId = stand.position.z < -150.0f
                ? "APRON_MID"
                : "APRON_WING_" +
                    (stand.position.z < -120.0f ? std::string("NORTH") :
                     stand.position.z < -80.0f ? std::string("MIDDLE") : std::string("SOUTH"));
            Connect(approachId, stand.id, 9.0f);
        }
    }

    // A cross-apron lane feeds the four stands facing the terminal's main hall.
    const float backRowX[] = {-33.75f, -11.25f, 11.25f, 33.75f};
    const std::string backRowNodeIds[] = {"BACK_ACCESS_W2", "BACK_ACCESS_W1", "BACK_ACCESS_E1", "BACK_ACCESS_E2"};
    for (int i = 0; i < 4; ++i)
        AddNode(backRowNodeIds[i], {backRowX[i], 0.08f, -45.0f});
    Connect("APRON_BACK_ROW", backRowNodeIds[1], 8.0f);
    Connect(backRowNodeIds[1], backRowNodeIds[0], 8.0f);
    Connect("APRON_BACK_ROW", backRowNodeIds[2], 8.0f);
    Connect(backRowNodeIds[2], backRowNodeIds[3], 8.0f);

    for (int i = 0; i < 4; ++i)
    {
        const AirportParkingStand& stand = stands[static_cast<std::size_t>(i)];
        Connect(backRowNodeIds[i], stand.id, 7.0f);
    }
}

void AirportGround::AddNode(const std::string& id, Vector3 position)
{
    nodes.push_back({id, position, {}});
}

void AirportGround::Connect(const std::string& from, const std::string& to, float width, bool drawSurface)
{
    TaxiwayNode* fromNode = nullptr;
    TaxiwayNode* toNode = nullptr;
    for (TaxiwayNode& node : nodes)
    {
        if (node.id == from) fromNode = &node;
        if (node.id == to) toNode = &node;
    }
    if (fromNode == nullptr || toNode == nullptr) return;

    fromNode->connectedNodeIds.push_back(to);
    toNode->connectedNodeIds.push_back(from);
    edges.push_back({from, to, width, drawSurface});
}

const TaxiwayNode* AirportGround::FindNode(const std::string& id) const
{
    const auto found = std::find_if(nodes.begin(), nodes.end(),
        [&id](const TaxiwayNode& node) { return node.id == id; });
    return found == nodes.end() ? nullptr : &*found;
}

void AirportGround::Draw() const
{
    // The terminal provides the apron slab. Add grass and hard surface around it.
    DrawPlane({0.0f, -0.04f, -360.0f}, {1500.0f, 900.0f}, Grass);
    DrawRunway();

    for (const TaxiwayEdge& edge : edges)
    {
        if (!edge.drawSurface) continue;
        const TaxiwayNode* from = FindNode(edge.fromNodeId);
        const TaxiwayNode* to = FindNode(edge.toNodeId);
        if (from == nullptr || to == nullptr) continue;

        const float dx = to->position.x - from->position.x;
        const float dz = to->position.z - from->position.z;
        const Vector3 center{
            (from->position.x + to->position.x) * 0.5f,
            0.055f,
            (from->position.z + to->position.z) * 0.5f
        };
        const Vector3 size = std::fabs(dx) >= std::fabs(dz)
            ? Vector3{std::fabs(dx) + edge.width, 0.12f, edge.width}
            : Vector3{edge.width, 0.12f, std::fabs(dz) + edge.width};
        DrawCubeV({center.x, 0.015f, center.z}, {size.x + 8.0f, size.y, size.z + 8.0f}, TaxiwayShoulder);
        DrawCubeV(center, size, TaxiwaySurface);
        DrawLine3D({from->position.x, 0.12f, from->position.z},
            {to->position.x, 0.12f, to->position.z}, TaxiwayCenterline);
    }

    // Intersections are visually distinct and are the route graph's decision points.
    for (const TaxiwayNode& node : nodes)
    {
        if (node.id.rfind("RWY_EXIT_", 0) == 0 || node.id.rfind("A_EXIT_", 0) == 0 ||
            node.id == "A_CENTER" || node.id == "APRON_ENTRY" || node.id == "APRON_BACK_ROW")
            DrawCylinder({node.position.x, 0.11f, node.position.z}, 2.0f, 2.0f, 0.08f, 10, IntersectionMarking);
    }

    // Hold-short bars at each runway access point.
    const float runwayHoldPositions[] = {-400.0f, 0.0f, 400.0f};
    for (float x : runwayHoldPositions)
    {
        DrawLine3D({x - 9.0f, 0.14f, -535.0f}, {x + 9.0f, 0.14f, -535.0f}, RunwayMarking);
        DrawLine3D({x - 9.0f, 0.14f, -531.0f}, {x + 9.0f, 0.14f, -531.0f}, RunwayMarking);
    }
}
