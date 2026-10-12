#include "airport_ground.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>

namespace
{
    constexpr Color Grass{78, 112, 91, 255};
    constexpr Color RunwayShoulder{79, 86, 89, 255};
    constexpr Color RunwaySurface{48, 55, 60, 255};
    constexpr Color TaxiwayShoulder{86, 94, 95, 255};
    constexpr Color TaxiwaySurface{75, 83, 85, 255};
    constexpr Color TaxiwayCenterline{241, 195, 70, 255};
    constexpr Color RunwayMarking{235, 236, 226, 255};

    void DrawQuad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color)
    {
        DrawTriangle3D(a, b, c, color);
        DrawTriangle3D(a, c, d, color);
    }

    void DrawFlatSegment(Vector3 from, Vector3 to, float width, float y, Color color)
    {
        const float dx = to.x - from.x;
        const float dz = to.z - from.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        if (length < 0.01f) return;

        const float halfWidth = width * 0.5f;
        const float nx = -dz / length * halfWidth;
        const float nz = dx / length * halfWidth;
        DrawQuad({from.x + nx, y, from.z + nz}, {to.x + nx, y, to.z + nz},
            {to.x - nx, y, to.z - nz}, {from.x - nx, y, from.z - nz}, color);
    }

    void DrawGroundAroundApron()
    {
        // Keep the terminal's original grey slab visible without a coplanar
        // ground plane under it. These sections meet, but do not overlap, the slab.
        constexpr float groundY = -0.01f;
        constexpr float groundMinX = -750.0f;
        constexpr float groundMaxX = 750.0f;
        constexpr float groundMinZ = -810.0f;
        constexpr float groundMaxZ = 155.0f;
        constexpr float slabMinX = -210.0f;
        constexpr float slabMaxX = 210.0f;
        constexpr float slabMinZ = -265.0f;

        const float groundLength = groundMaxZ - groundMinZ;
        const float sideWidth = slabMinX - groundMinX;
        DrawPlane({(groundMinX + slabMinX) * 0.5f, groundY,
                      (groundMinZ + groundMaxZ) * 0.5f},
            {sideWidth, groundLength}, Grass);
        DrawPlane({(slabMaxX + groundMaxX) * 0.5f, groundY,
                      (groundMinZ + groundMaxZ) * 0.5f},
            {sideWidth, groundLength}, Grass);

        const float rearLength = slabMinZ - groundMinZ;
        DrawPlane({0.0f, groundY, (groundMinZ + slabMinZ) * 0.5f},
            {slabMaxX - slabMinX, rearLength}, Grass);
    }

    void DrawRunway(const AirportRunway& runway)
    {
        const float centerX = runway.center.x;
        const float centerZ = runway.center.z;
        DrawCubeV({centerX, -0.015f, centerZ},
            // Keep the grey runway base and runway surface aligned in plan.
            {runway.length, 0.09f, runway.width}, RunwayShoulder);
        DrawCubeV({centerX, 0.04f, centerZ},
            {runway.length, 0.12f, runway.width}, RunwaySurface);

        const float edgeX = runway.length * 0.5f - 6.0f;
        const float northEdge = centerZ - runway.width * 0.5f + 2.5f;
        const float southEdge = centerZ + runway.width * 0.5f - 2.5f;
        DrawFlatSegment({centerX - edgeX, 0.0f, northEdge},
            {centerX + edgeX, 0.0f, northEdge}, 0.325f, 0.108f, RunwayMarking);
        DrawFlatSegment({centerX - edgeX, 0.0f, southEdge},
            {centerX + edgeX, 0.0f, southEdge}, 0.325f, 0.108f, RunwayMarking);

        const float halfLength = runway.length * 0.5f;
        const float aimingPointDistance = halfLength - 37.5f;
        for (float offset = -halfLength + 25.0f;
             offset + 10.0f <= halfLength - 25.0f; offset += 26.0f)
        {
            const float dashCenter = offset + 5.0f;
            if (std::fabs(dashCenter + aimingPointDistance) < 17.0f ||
                std::fabs(dashCenter - aimingPointDistance) < 17.0f) continue;
            DrawFlatSegment({centerX + offset, 0.0f, centerZ},
                {centerX + offset + 10.0f, 0.0f, centerZ}, 0.3f, 0.109f, RunwayMarking);
        }

        // Threshold bars run lengthwise and are evenly spaced across the runway.
        for (int i = 0; i < 6; ++i)
        {
            const float z = centerZ + (static_cast<float>(i) - 2.5f) * 2.85f;
            const float leftX = centerX - halfLength + 12.0f;
            const float rightX = centerX + halfLength - 12.0f;
            DrawFlatSegment({leftX - 5.5f, 0.0f, z}, {leftX + 5.5f, 0.0f, z},
                1.25f, 0.11f, RunwayMarking);
            DrawFlatSegment({rightX - 5.5f, 0.0f, z}, {rightX + 5.5f, 0.0f, z},
                1.25f, 0.11f, RunwayMarking);
        }

        for (int side : {-1, 1})
        {
            const float x = centerX + side * (halfLength - 37.5f);
            for (int zSide : {-1, 1})
                DrawFlatSegment({x - 7.5f, 0.0f, centerZ + zSide * 5.0f},
                    {x + 7.5f, 0.0f, centerZ + zSide * 5.0f},
                    1.4f, 0.11f, RunwayMarking);
        }
    }

    void DrawTaxiwaySurface(Vector3 from, Vector3 to, float width)
    {
        DrawFlatSegment(from, to, width + 4.0f, 0.012f, TaxiwayShoulder);
        DrawFlatSegment(from, to, width, 0.026f, TaxiwaySurface);
    }

    void DrawTaxiwayCenterline(Vector3 from, Vector3 to,
        bool stopAtHoldLine, float holdLineZ = -517.0f)
    {
        if (stopAtHoldLine)
        {
            // Runway connector edges are stored from runway exit to taxiway,
            // so trim the line at the runway end and leave the taxiway portion.
            from.z = holdLineZ;
        }

        DrawFlatSegment(from, to, 0.31f, 0.075f, TaxiwayCenterline);
    }

}

AirportGround::AirportGround(const AirportTerminal& terminal)
{
    // Three shorter parallel runways keep a visible grass buffer between
    // each runway and the inter-runway taxi lanes.
    runways = {
        {"RUNWAY_A", {0.0f, 0.0f, -507.0f}, 300.0f, 26.0f},
        {"RUNWAY_B", {0.0f, 0.0f, -642.0f}, 300.0f, 26.0f},
        {"RUNWAY_C", {0.0f, 0.0f, -777.0f}, 300.0f, 26.0f}
    };

    constexpr float routeY = 0.08f;
    constexpr float runwayExitXs[] = {-65.0f, 0.0f, 65.0f};
    constexpr char runwaySuffixes[] = {'W', 'C', 'E'};
    const std::string runwayPrefixes[] = {"RUNWAY_A", "RUNWAY_B", "RUNWAY_C"};

    // Keep route nodes along each full runway centreline, with usable exits at
    // west, centre and east. Runway A's original IDs remain available.
    for (std::size_t r = 0; r < runways.size(); ++r)
    {
        const float z = runways[r].center.z;
        const float halfLength = runways[r].length * 0.5f - 10.0f;
        const std::string prefix = r == 0 ? std::string("RUNWAY") : runwayPrefixes[r];
        const std::string westEnd = prefix + "_START_W";
        const std::string eastEnd = prefix + "_END_E";
        AddNode(westEnd, {-halfLength, routeY, z});
        AddNode(prefix + "_W", {runwayExitXs[0], routeY, z});
        AddNode(prefix + "_CENTER", {runwayExitXs[1], routeY, z});
        AddNode(prefix + "_E", {runwayExitXs[2], routeY, z});
        AddNode(eastEnd, {halfLength, routeY, z});
        Connect(westEnd, prefix + "_W", 1.0f, false);
        Connect(prefix + "_W", prefix + "_CENTER", 1.0f, false);
        Connect(prefix + "_CENTER", prefix + "_E", 1.0f, false);
        Connect(prefix + "_E", eastEnd, 1.0f, false);
    }

    // Taxiway A is the primary parallel taxiway between Runway A and apron.
    constexpr float parallelTaxiZ = -425.0f;
    AddNode("A_WEST", {-280.0f, routeY, parallelTaxiZ});
    AddNode("A_EXIT_W", {-65.0f, routeY, parallelTaxiZ});
    AddNode("A_CENTER", {0.0f, routeY, parallelTaxiZ});
    AddNode("A_EXIT_E", {65.0f, routeY, parallelTaxiZ});
    AddNode("A_EAST", {280.0f, routeY, parallelTaxiZ});
    Connect("A_WEST", "A_EXIT_W", 15.0f);
    Connect("A_EXIT_W", "A_CENTER", 15.0f);
    Connect("A_CENTER", "A_EXIT_E", 15.0f);
    Connect("A_EXIT_E", "A_EAST", 15.0f);

    const float runwayNorthEdges[] = {
        runways[0].center.z - runways[0].width * 0.5f,
        runways[1].center.z - runways[1].width * 0.5f,
        runways[2].center.z - runways[2].width * 0.5f};
    const float runwaySouthEdges[] = {
        runways[0].center.z + runways[0].width * 0.5f,
        runways[1].center.z + runways[1].width * 0.5f,
        runways[2].center.z + runways[2].width * 0.5f};
    const float pairTaxiZs[] = {
        (runwayNorthEdges[0] + runwaySouthEdges[1]) * 0.5f,
        (runwayNorthEdges[1] + runwaySouthEdges[2]) * 0.5f};
    const char* pairPrefixes[] = {"AB", "BC"};
    const int upperRunways[] = {0, 1};
    const int lowerRunways[] = {1, 2};

    auto makeBezierConnection = [this](const std::string& pathId,
        const std::string& fromId, const std::string& toId,
        Vector3 control1, Vector3 control2, float width)
    {
        const TaxiwayNode* fromNode = FindNode(fromId);
        const TaxiwayNode* toNode = FindNode(toId);
        if (fromNode == nullptr || toNode == nullptr) return;
        const Vector3 start = fromNode->position;
        const Vector3 finish = toNode->position;
        std::string previousId = fromId;
        constexpr int segments = 8;
        for (int step = 1; step < segments; ++step)
        {
            const float t = static_cast<float>(step) / segments;
            const float oneMinusT = 1.0f - t;
            const float x = oneMinusT * oneMinusT * oneMinusT * start.x +
                3.0f * oneMinusT * oneMinusT * t * control1.x +
                3.0f * oneMinusT * t * t * control2.x + t * t * t * finish.x;
            const float z = oneMinusT * oneMinusT * oneMinusT * start.z +
                3.0f * oneMinusT * oneMinusT * t * control1.z +
                3.0f * oneMinusT * t * t * control2.z + t * t * t * finish.z;
            const std::string nodeId = pathId + "_" + std::to_string(step);
            AddNode(nodeId, {x, start.y, z});
            Connect(previousId, nodeId, width);
            previousId = nodeId;
        }
        Connect(previousId, toId, width);
    };

    // Shared, parallel taxiways in the grass-separated runway corridors.
    for (int pair = 0; pair < 2; ++pair)
    {
        const std::string prefix = pairPrefixes[pair];
        const float taxiZ = pairTaxiZs[pair];
        const std::string west = prefix + "_TAXI_W";
        const std::string exitWest = prefix + "_EXIT_W";
        const std::string center = prefix + "_CENTER";
        const std::string exitEast = prefix + "_EXIT_E";
        const std::string east = prefix + "_TAXI_E";
        AddNode(west, {-240.0f, routeY, taxiZ});
        AddNode(exitWest, {runwayExitXs[0], routeY, taxiZ});
        AddNode(center, {0.0f, routeY, taxiZ});
        AddNode(exitEast, {runwayExitXs[2], routeY, taxiZ});
        AddNode(east, {240.0f, routeY, taxiZ});
        Connect(west, exitWest, 12.0f);
        Connect(exitWest, center, 12.0f);
        Connect(center, exitEast, 12.0f);
        Connect(exitEast, east, 12.0f);

        for (int i = 0; i < 3; ++i)
        {
            const int upper = upperRunways[pair];
            const int lower = lowerRunways[pair];
            const std::string upperExit = prefix + "_UPPER_EXIT_" + runwaySuffixes[i];
            const std::string lowerExit = prefix + "_LOWER_EXIT_" + runwaySuffixes[i];
            const std::string exitPosition = i == 0 ? "W" : i == 1 ? "CENTER" : "E";
            const std::string upperPrefix = upper == 0 ? std::string("RUNWAY") : runwayPrefixes[upper];
            const std::string upperRunwayNode = upperPrefix + "_" + exitPosition;
            const std::string lowerRunwayNode = runwayPrefixes[lower] + "_" + exitPosition;
            const float upperEdge = runwayNorthEdges[upper];
            const float lowerEdge = runwaySouthEdges[lower];

            AddNode(upperExit, {runwayExitXs[i], routeY, upperEdge});
            AddNode(lowerExit, {runwayExitXs[i], routeY, lowerEdge});
            Connect(upperRunwayNode, upperExit, 1.0f, false);
            Connect(lowerRunwayNode, lowerExit, 1.0f, false);
            Connect(upperExit, i == 0 ? exitWest : i == 1 ? center : exitEast,
                12.0f, true, true, upperEdge - 6.0f);
            Connect(lowerExit, i == 0 ? exitWest : i == 1 ? center : exitEast,
                12.0f, true, true, lowerEdge + 6.0f);
        }
    }

    // Runway A exits to its south-side parallel taxiway.
    const char* runwayAExits[] = {"RWY_EXIT_W", "RWY_EXIT_C", "RWY_EXIT_E"};
    const char* runwayAPathNodes[] = {"RUNWAY_W", "RUNWAY_CENTER", "RUNWAY_E"};
    const char* parallelNodes[] = {"A_EXIT_W", "A_CENTER", "A_EXIT_E"};
    for (int i = 0; i < 3; ++i)
    {
        AddNode(runwayAExits[i], {runwayExitXs[i], routeY, runwaySouthEdges[0]});
        Connect(runwayAPathNodes[i], runwayAExits[i], 1.0f, false);
        Connect(runwayAExits[i], parallelNodes[i], 15.0f, true, true,
            runwaySouthEdges[0] + 6.0f);
    }

    // The eastern bypass gives traffic a route between the parallel taxiways.
    AddNode("BYPASS_A", {310.0f, routeY, parallelTaxiZ});
    AddNode("BYPASS_AB", {310.0f, routeY, pairTaxiZs[0]});
    AddNode("BYPASS_BC", {310.0f, routeY, pairTaxiZs[1]});
    Connect("A_EAST", "BYPASS_A", 12.0f);
    Connect("BYPASS_A", "BYPASS_AB", 11.0f);
    Connect("BYPASS_AB", "BYPASS_BC", 11.0f);
    makeBezierConnection("BYPASS_AB_TURN", "BYPASS_AB", "AB_TAXI_E",
        {310.0f, routeY, pairTaxiZs[0] + 17.5f},
        {280.0f, routeY, pairTaxiZs[0]}, 11.0f);
    makeBezierConnection("BYPASS_BC_TURN", "BYPASS_BC", "BC_TAXI_E",
        {310.0f, routeY, pairTaxiZs[1] + 17.5f},
        {280.0f, routeY, pairTaxiZs[1]}, 11.0f);

    // Preserve the approach from the primary taxiway into the existing apron.
    AddNode("APRON_ENTRY", {0.0f, routeY, -220.0f});
    AddNode("APRON_GATEWAY", {0.0f, routeY, -190.0f});
    AddNode("APRON_SIDE_FAR", {0.0f, routeY, -170.0f});
    AddNode("WING_SPINE_3", {0.0f, routeY, -162.0f});
    AddNode("APRON_SIDE_MIDDLE", {0.0f, routeY, -130.0f});
    AddNode("WING_SPINE_2", {0.0f, routeY, -122.0f});
    AddNode("APRON_SIDE_NEAR", {0.0f, routeY, -90.0f});
    AddNode("WING_SPINE_1", {0.0f, routeY, -82.0f});
    AddNode("APRON_BACK_ROW", {0.0f, routeY, -48.0f});
    Connect("A_CENTER", "APRON_ENTRY", 15.0f);
    Connect("APRON_ENTRY", "APRON_GATEWAY", 14.0f);
    Connect("APRON_GATEWAY", "APRON_SIDE_FAR", 12.0f);
    Connect("APRON_SIDE_FAR", "WING_SPINE_3", 11.0f);
    Connect("WING_SPINE_3", "APRON_SIDE_MIDDLE", 11.0f);
    Connect("APRON_SIDE_MIDDLE", "WING_SPINE_2", 11.0f);
    Connect("WING_SPINE_2", "APRON_SIDE_NEAR", 11.0f);
    Connect("APRON_SIDE_NEAR", "WING_SPINE_1", 10.0f);
    Connect("WING_SPINE_1", "APRON_BACK_ROW", 10.0f);

    // A separate gate-side apron lane gives all four facade stands an approach.
    constexpr float gateXs[] = {-33.75f, -11.25f, 11.25f, 33.75f};
    AddNode("GATE_ROW_W", {-75.0f, routeY, -48.0f});
    AddNode("GATE_ROW_1", {gateXs[0], routeY, -48.0f});
    AddNode("GATE_ROW_2", {gateXs[1], routeY, -48.0f});
    AddNode("GATE_ROW_3", {gateXs[2], routeY, -48.0f});
    AddNode("GATE_ROW_4", {gateXs[3], routeY, -48.0f});
    AddNode("GATE_ROW_E", {75.0f, routeY, -48.0f});
    Connect("GATE_ROW_W", "GATE_ROW_1", 10.0f);
    Connect("GATE_ROW_1", "GATE_ROW_2", 10.0f);
    Connect("GATE_ROW_2", "APRON_BACK_ROW", 10.0f);
    Connect("APRON_BACK_ROW", "GATE_ROW_3", 10.0f);
    Connect("GATE_ROW_3", "GATE_ROW_4", 10.0f);
    Connect("GATE_ROW_4", "GATE_ROW_E", 10.0f);

    // Taxiway branches reach the six wing stands without crossing the stands.
    const float wingTaxiZs[] = {-82.0f, -122.0f, -162.0f};
    for (int row = 0; row < 3; ++row)
    {
        const float z = wingTaxiZs[row];
        const std::string rowId = "WING_ROW_" + std::to_string(row + 1);
        const std::string westId = rowId + "_W";
        const std::string eastId = rowId + "_E";
        AddNode(westId, {-15.0f, routeY, z});
        AddNode(eastId, {15.0f, routeY, z});
        const std::string spineId = row == 0 ? "WING_SPINE_1" :
            row == 1 ? "WING_SPINE_2" : "WING_SPINE_3";
        Connect(spineId, westId, 9.0f);
        Connect(spineId, eastId, 9.0f);
        Connect(westId, eastId, 9.0f);
    }

    // Join route nodes to the terminal's existing painted parking stands.
    for (const AirportParkingStand& stand : terminal.GetParkingStands())
    {
        Vector3 standPosition = stand.position;
        standPosition.y = routeY;
        AddNode(stand.id, standPosition);

        if (stand.id.rfind("GATE_", 0) == 0)
        {
            const float entryZ = standPosition.z - 6.0f;
            const std::string entryId = stand.id + "_ENTRY";
            AddNode(entryId, {standPosition.x, routeY, entryZ});
            const std::string rowIndex = stand.id.substr(5);
            Connect("GATE_ROW_" + rowIndex, entryId, 9.0f);
            Connect(entryId, stand.id, 1.0f, false);
        }
        else
        {
            const bool westStand = stand.id.rfind("WEST_", 0) == 0;
            const float entryX = standPosition.x + (westStand ? 6.25f : -6.25f);
            const std::string entryId = stand.id + "_ENTRY";
            AddNode(entryId, {entryX, routeY, standPosition.z});
            const std::string rowIndex = stand.id.substr(5);
            const std::string rowId = "WING_ROW_" + rowIndex + (westStand ? "_W" : "_E");
            Connect(rowId, entryId, 9.0f);
            Connect(entryId, stand.id, 1.0f, false);
        }
    }
}

void AirportGround::AddNode(const std::string& id, Vector3 position)
{
    nodes.push_back({id, position, {}});
}

void AirportGround::Connect(const std::string& from, const std::string& to, float width,
    bool drawSurface, bool stopAtHoldLine, float holdLineZ)
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
    edges.push_back({from, to, width, drawSurface, stopAtHoldLine, holdLineZ});
}

const TaxiwayNode* AirportGround::FindNode(const std::string& id) const
{
    const auto found = std::find_if(nodes.begin(), nodes.end(),
        [&id](const TaxiwayNode& node) { return node.id == id; });
    return found == nodes.end() ? nullptr : &*found;
}

void AirportGround::Draw() const
{
    // Ground remains outside the terminal slab to avoid coplanar flicker.
    DrawGroundAroundApron();
    for (const AirportRunway& runway : runways)
        DrawRunway(runway);

    for (const TaxiwayEdge& edge : edges)
    {
        if (!edge.drawSurface) continue;
        const TaxiwayNode* from = FindNode(edge.fromNodeId);
        const TaxiwayNode* to = FindNode(edge.toNodeId);
        if (from == nullptr || to == nullptr) continue;

        DrawTaxiwaySurface(from->position, to->position, edge.width);
    }

    // Round pavement at three-way and four-way intersections. The pads share
    // the lanes' elevations and colors, so the surfaces join without a curb.
    for (const TaxiwayNode& node : nodes)
    {
        std::vector<const TaxiwayNode*> pavementNeighbors;
        float widestLane = 0.0f;
        for (const TaxiwayEdge& edge : edges)
        {
            if (!edge.drawSurface) continue;
            if (edge.fromNodeId == node.id)
            {
                pavementNeighbors.push_back(FindNode(edge.toNodeId));
                widestLane = std::max(widestLane, edge.width);
            }
            else if (edge.toNodeId == node.id)
            {
                pavementNeighbors.push_back(FindNode(edge.fromNodeId));
                widestLane = std::max(widestLane, edge.width);
            }
        }
        if (pavementNeighbors.size() < 2 || widestLane <= 0.0f) continue;

        const bool isBezierSample = node.id.rfind("BYPASS_AB_TURN_", 0) == 0 ||
            node.id.rfind("BYPASS_BC_TURN_", 0) == 0;
        bool needsJoinPad = pavementNeighbors.size() >= 3 || isBezierSample;
        if (pavementNeighbors.size() == 2 && pavementNeighbors[0] && pavementNeighbors[1])
        {
            const float ax = pavementNeighbors[0]->position.x - node.position.x;
            const float az = pavementNeighbors[0]->position.z - node.position.z;
            const float bx = pavementNeighbors[1]->position.x - node.position.x;
            const float bz = pavementNeighbors[1]->position.z - node.position.z;
            const float aLength = std::sqrt(ax * ax + az * az);
            const float bLength = std::sqrt(bx * bx + bz * bz);
            if (aLength > 0.01f && bLength > 0.01f)
                needsJoinPad = (ax * bx + az * bz) / (aLength * bLength) < 0.85f;
        }
        if (!needsJoinPad) continue;

        DrawCylinder({node.position.x, -0.015f, node.position.z},
            (widestLane + 4.0f) * 0.5f, (widestLane + 4.0f) * 0.5f,
            0.027f, 20, TaxiwayShoulder);
        DrawCylinder({node.position.x, -0.02f, node.position.z},
            widestLane * 0.5f, widestLane * 0.5f, 0.046f, 20, TaxiwaySurface);
    }

    // Paint continuous centerlines after the intersection pads so they remain
    // unbroken through every join. Runway exits stop at the holding position.
    for (const TaxiwayEdge& edge : edges)
    {
        if (!edge.drawSurface) continue;
        const TaxiwayNode* from = FindNode(edge.fromNodeId);
        const TaxiwayNode* to = FindNode(edge.toNodeId);
        if (from == nullptr || to == nullptr) continue;
        DrawTaxiwayCenterline(from->position, to->position,
            edge.stopAtHoldLine, edge.holdLineZ);
    }

    // Simplified runway holding-position marking. The solid pair faces the
    // taxiway approach; dashed bars sit beyond it on the runway side.
    const auto drawHoldMarking = [](float x, float solidOne, float solidTwo,
        float dashedOne, float dashedTwo)
    {
        constexpr float lineY = 0.135f;
        constexpr float lineThickness = 0.025f;
        constexpr float lineDepth = 0.25f;
        constexpr Color HoldMarking{245, 197, 55, 255};
        DrawCubeV({x, lineY, solidOne}, {13.5f, lineThickness, lineDepth}, HoldMarking);
        DrawCubeV({x, lineY, solidTwo}, {13.5f, lineThickness, lineDepth}, HoldMarking);

        for (float z : {dashedOne, dashedTwo})
        {
            for (int dash = 0; dash < 7; ++dash)
            {
                const float dashX = x - 6.0f + dash * 2.0f;
                DrawCubeV({dashX, lineY, z}, {1.0f, lineThickness, lineDepth}, HoldMarking);
            }
        }
    };

    const auto drawRunwayHold = [&drawHoldMarking](float x, float runwayEdgeZ, float taxiSide)
    {
        drawHoldMarking(x,
            runwayEdgeZ + taxiSide * 4.0f,
            runwayEdgeZ + taxiSide * 5.25f,
            runwayEdgeZ + taxiSide * 2.75f,
            runwayEdgeZ + taxiSide * 1.5f);
    };
    const float runwayExitXs[] = {-65.0f, 0.0f, 65.0f};
    for (float x : runwayExitXs)
    {
        drawRunwayHold(x, runways[0].center.z + runways[0].width * 0.5f, 1.0f);
        drawRunwayHold(x, runways[0].center.z - runways[0].width * 0.5f, -1.0f);
        drawRunwayHold(x, runways[1].center.z + runways[1].width * 0.5f, 1.0f);
        drawRunwayHold(x, runways[1].center.z - runways[1].width * 0.5f, -1.0f);
        drawRunwayHold(x, runways[2].center.z + runways[2].width * 0.5f, 1.0f);
    }
}
