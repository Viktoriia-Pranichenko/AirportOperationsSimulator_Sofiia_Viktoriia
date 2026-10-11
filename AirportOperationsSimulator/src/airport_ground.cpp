#include "airport_ground.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
    constexpr Color Grass{78, 112, 91, 255};
    constexpr Color RunwayShoulder{79, 86, 89, 255};
    constexpr Color RunwaySurface{48, 55, 60, 255};
    constexpr Color TaxiwayShoulder{86, 94, 95, 255};
    constexpr Color TaxiwaySurface{75, 83, 85, 255};
    constexpr Color TaxiwayCenterline{241, 195, 70, 255};
    constexpr Color RunwayMarking{235, 236, 226, 255};
    constexpr Color IntersectionMarking{255, 220, 105, 255};

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
        DrawCubeV({centerX, -0.03f, centerZ},
            {runway.length + 28.0f, 0.12f, runway.width + 28.0f}, RunwayShoulder);
        DrawCubeV({centerX, 0.05f, centerZ},
            {runway.length, 0.12f, runway.width}, RunwaySurface);

        const float edgeX = runway.length * 0.5f - 18.0f;
        const float northEdge = centerZ - runway.width * 0.5f + 9.0f;
        const float southEdge = centerZ + runway.width * 0.5f - 9.0f;
        DrawLine3D({centerX - edgeX, 0.12f, northEdge}, {centerX + edgeX, 0.12f, northEdge}, RunwayMarking);
        DrawLine3D({centerX - edgeX, 0.12f, southEdge}, {centerX + edgeX, 0.12f, southEdge}, RunwayMarking);

        const float halfLength = runway.length * 0.5f;
        for (float offset = -halfLength + 20.0f; offset + 34.0f <= halfLength - 20.0f; offset += 72.0f)
            DrawLine3D({centerX + offset, 0.13f, centerZ},
                {centerX + offset + 34.0f, 0.13f, centerZ}, RunwayMarking);

        for (int bar = 0; bar < 6; ++bar)
        {
            const float leftX = centerX - runway.length * 0.5f + 20.0f + bar * 7.0f;
            const float rightX = centerX + runway.length * 0.5f - 24.0f - bar * 7.0f;
            for (int side = -1; side <= 1; side += 2)
            {
                const float z = centerZ + side * (14.0f + bar * 4.0f);
                DrawLine3D({leftX, 0.13f, z}, {leftX, 0.13f, z + side * 12.0f}, RunwayMarking);
                DrawLine3D({rightX, 0.13f, z}, {rightX, 0.13f, z + side * 12.0f}, RunwayMarking);
            }
        }

        // Preserve the existing white threshold markings at both runway ends.
        for (int i = 0; i < 8; ++i)
        {
            const float z = centerZ - 25.0f + i * 7.0f;
            const float leftStart = centerX - runway.length * 0.5f + 24.0f + (i % 2) * 4.0f;
            const float rightStart = centerX + runway.length * 0.5f - 32.0f - (i % 2) * 4.0f;
            DrawLine3D({leftStart, 0.13f, z}, {leftStart + 23.0f, 0.13f, z}, RunwayMarking);
            DrawLine3D({rightStart - 23.0f, 0.13f, z}, {rightStart, 0.13f, z}, RunwayMarking);
        }
    }

    void DrawTaxiwaySurface(Vector3 from, Vector3 to, float width)
    {
        const float dx = to.x - from.x;
        const float dz = to.z - from.z;
        const float length = std::fabs(dx) + std::fabs(dz);
        const bool horizontal = std::fabs(dx) >= std::fabs(dz);
        const Vector3 center{(from.x + to.x) * 0.5f, 0.04f, (from.z + to.z) * 0.5f};

        // Each segment extends half a lane width past both nodes. Equal widths
        // make the joins meet cleanly; junction pads below soften the corners.
        const Vector3 shoulderSize = horizontal
            ? Vector3{length + width + 8.0f, 0.14f, width + 8.0f}
            : Vector3{width + 8.0f, 0.14f, length + width + 8.0f};
        const Vector3 pavementSize = horizontal
            ? Vector3{length + width, 0.14f, width}
            : Vector3{width, 0.14f, length + width};
        DrawCubeV(center, shoulderSize, TaxiwayShoulder);
        DrawCubeV({center.x, center.y + 0.004f, center.z}, pavementSize, TaxiwaySurface);
    }

    void DrawTaxiwayCenterline(Vector3 from, Vector3 to, float width,
        bool stopAtHoldLine, float holdLineZ = -517.0f)
    {
        if (stopAtHoldLine)
        {
            // Runway connector edges are stored from runway exit to taxiway,
            // so trim the line at the runway end and leave the taxiway portion.
            from.z = holdLineZ;
        }

        const float dx = to.x - from.x;
        const float dz = to.z - from.z;
        const bool horizontal = std::fabs(dx) >= std::fabs(dz);
        const float length = std::fabs(dx) + std::fabs(dz);
        const Vector3 center{(from.x + to.x) * 0.5f, 0.116f, (from.z + to.z) * 0.5f};
        const Vector3 stripeSize = horizontal
            ? Vector3{length + 0.4f, 0.025f, 0.42f}
            : Vector3{0.42f, 0.025f, length + 0.4f};
        DrawCubeV(center, stripeSize, TaxiwayCenterline);
    }

}

AirportGround::AirportGround(const AirportTerminal&)
{
    runways = {
        {"RUNWAY_A", {0.0f, 0.0f, -515.0f}, 700.0f, 60.0f},
        {"RUNWAY_B", {0.0f, 0.0f, -635.0f}, 700.0f, 60.0f},
        {"RUNWAY_C", {0.0f, 0.0f, -755.0f}, 700.0f, 60.0f}
    };

    // Keep Taxiway A in place and connect it to the south side of Runway A.
    AddNode("A_WEST", {-450.0f, 0.08f, -465.0f});
    AddNode("A_EXIT_W", {-260.0f, 0.08f, -465.0f});
    AddNode("A_CENTER", {0.0f, 0.08f, -465.0f});
    AddNode("A_EXIT_E", {260.0f, 0.08f, -465.0f});
    AddNode("A_EAST", {450.0f, 0.08f, -465.0f});
    Connect("A_WEST", "A_EXIT_W", 30.0f);
    Connect("A_EXIT_W", "A_CENTER", 30.0f);
    Connect("A_CENTER", "A_EXIT_E", 30.0f);
    Connect("A_EXIT_E", "A_EAST", 30.0f);

    const float taxiwayWidth = 24.0f;
    const float runwayExitXs[] = {-260.0f, 0.0f, 260.0f};
    const char* runwaySuffixes[] = {"W", "C", "E"};
    const std::string runwayPrefixes[] = {"RUNWAY_A", "RUNWAY_B", "RUNWAY_C"};

    // Route nodes along all runway centerlines remain separate from taxiway
    // pavement so later movement logic can reserve runway occupancy.
    for (std::size_t runwayIndex = 1; runwayIndex < runways.size(); ++runwayIndex)
    {
        const std::string& prefix = runwayPrefixes[runwayIndex];
        const float z = runways[runwayIndex].center.z;
        AddNode(prefix + "_W", {runwayExitXs[0], 0.08f, z});
        AddNode(prefix + "_CENTER", {runwayExitXs[1], 0.08f, z});
        AddNode(prefix + "_E", {runwayExitXs[2], 0.08f, z});
        Connect(prefix + "_W", prefix + "_CENTER", 1.0f, false);
        Connect(prefix + "_CENTER", prefix + "_E", 1.0f, false);
    }

    // Preserve the original Runway A node IDs for existing movement code.
    // (Runway A keeps its pre-existing RUNWAY_W/CENTER/E node names.)
    AddNode("RUNWAY_W", {runwayExitXs[0], 0.08f, runways[0].center.z});
    AddNode("RUNWAY_CENTER", {runwayExitXs[1], 0.08f, runways[0].center.z});
    AddNode("RUNWAY_E", {runwayExitXs[2], 0.08f, runways[0].center.z});
    Connect("RUNWAY_W", "RUNWAY_CENTER", 1.0f, false);
    Connect("RUNWAY_CENTER", "RUNWAY_E", 1.0f, false);

    const float runwayASouthEdge = runways[0].center.z + runways[0].width * 0.5f;
    const float runwayANorthEdge = runways[0].center.z - runways[0].width * 0.5f;
    const float runwayBSouthEdge = runways[1].center.z + runways[1].width * 0.5f;
    const float runwayBNorthEdge = runways[1].center.z - runways[1].width * 0.5f;
    const float runwayCSouthEdge = runways[2].center.z + runways[2].width * 0.5f;
    const float abTaxiZ = (runwayANorthEdge + runwayBSouthEdge) * 0.5f;
    const float bcTaxiZ = (runwayBNorthEdge + runwayCSouthEdge) * 0.5f;

    // Taxiway A joins Runway A's south side. Runway A's north and Runway B's
    // south exits share a taxiway between them; Runway B and C share another.
    const char* taxiPrefixes[] = {"AB", "BC"};
    const float taxiZs[] = {abTaxiZ, bcTaxiZ};
    const int upperRunwayIndices[] = {0, 1};
    const int lowerRunwayIndices[] = {1, 2};
    const auto runwayNodeId = [&runwayPrefixes, &runwaySuffixes](int runwayIndex, int exitIndex)
    {
        if (runwayIndex == 0)
            return std::string(exitIndex == 1 ? "RUNWAY_CENTER" :
                exitIndex == 0 ? "RUNWAY_W" : "RUNWAY_E");
        return runwayPrefixes[runwayIndex] + "_" +
            (exitIndex == 1 ? "CENTER" : runwaySuffixes[exitIndex]);
    };
    for (int pair = 0; pair < 2; ++pair)
    {
        const std::string prefix = taxiPrefixes[pair];
        const float taxiZ = taxiZs[pair];
        const int upper = upperRunwayIndices[pair];
        const int lower = lowerRunwayIndices[pair];
        const float upperEdge = runways[upper].center.z - runways[upper].width * 0.5f;
        const float lowerEdge = runways[lower].center.z + runways[lower].width * 0.5f;

        AddNode(prefix + "_TAXI_W", {-320.0f, 0.08f, taxiZ});
        AddNode(prefix + "_TAXI_E", {320.0f, 0.08f, taxiZ});
        const std::string laneNodes[] = {
            prefix + "_TAXI_W", prefix + "_EXIT_W",
            prefix + "_CENTER", prefix + "_EXIT_E", prefix + "_TAXI_E"
        };
        const float laneXs[] = {-320.0f, -260.0f, 0.0f, 260.0f, 320.0f};
        for (int i = 1; i < 4; ++i)
            AddNode(laneNodes[i], {laneXs[i], 0.08f, taxiZ});
        for (int i = 0; i < 4; ++i)
            Connect(laneNodes[i], laneNodes[i + 1], taxiwayWidth);

        for (int i = 0; i < 3; ++i)
        {
            const std::string upperExit = prefix + "_UPPER_EXIT_" + runwaySuffixes[i];
            const std::string lowerExit = prefix + "_LOWER_EXIT_" + runwaySuffixes[i];
            const std::string upperRunwayNode = runwayNodeId(upper, i);
            const std::string lowerRunwayNode = runwayNodeId(lower, i);

            AddNode(upperExit, {runwayExitXs[i], 0.08f, upperEdge});
            AddNode(lowerExit, {runwayExitXs[i], 0.08f, lowerEdge});
            Connect(upperRunwayNode, upperExit, 1.0f, false);
            Connect(lowerRunwayNode, lowerExit, 1.0f, false);
            Connect(upperExit, laneNodes[i + 1], taxiwayWidth, true, true, upperEdge - 13.0f);
            Connect(lowerExit, laneNodes[i + 1], taxiwayWidth, true, true, lowerEdge + 13.0f);
        }
    }

    // Three runway ends feed a shared east-side bypass. It runs outside the
    // runway shoulders and joins the existing Taxiway A without crossing a runway.
    AddNode("BYPASS_A", {390.0f, 0.08f, -465.0f});
    AddNode("BYPASS_AB", {390.0f, 0.08f, abTaxiZ});
    AddNode("BYPASS_BC", {390.0f, 0.08f, bcTaxiZ});
    Connect("A_EAST", "BYPASS_A", 30.0f);
    Connect("BYPASS_A", "BYPASS_AB", 20.0f);
    Connect("BYPASS_AB", "BYPASS_BC", 20.0f);
    Connect("BYPASS_AB", "AB_TAXI_E", 24.0f);
    Connect("BYPASS_BC", "BC_TAXI_E", 24.0f);

    // Runway A retains the original south-side exit IDs for existing logic.
    const char* runwayAExits[] = {"RWY_EXIT_W", "RWY_EXIT_C", "RWY_EXIT_E"};
    const char* runwayAPathNodes[] = {"RUNWAY_W", "RUNWAY_CENTER", "RUNWAY_E"};
    for (int i = 0; i < 3; ++i)
    {
        AddNode(runwayAExits[i], {runwayExitXs[i], 0.08f, runwayASouthEdge});
        Connect(runwayAPathNodes[i], runwayAExits[i], 1.0f, false);
        const char* taxiNode = i == 0 ? "A_EXIT_W" : i == 1 ? "A_CENTER" : "A_EXIT_E";
        Connect(runwayAExits[i], taxiNode, 30.0f, true, true, runwayASouthEdge + 13.0f);
    }

    // Broad connector enters the existing paved terminal apron, then narrows
    // gradually into the apron movement lane.
    AddNode("APRON_ENTRY", {0.0f, 0.08f, -220.0f});
    AddNode("APRON_GATEWAY", {0.0f, 0.08f, -190.0f});
    AddNode("APRON_SIDE_FAR", {0.0f, 0.08f, -170.0f});
    AddNode("APRON_SIDE_MIDDLE", {0.0f, 0.08f, -130.0f});
    AddNode("APRON_SIDE_NEAR", {0.0f, 0.08f, -90.0f});
    AddNode("APRON_BACK_ROW", {0.0f, 0.08f, -54.375f});
    Connect("A_CENTER", "APRON_ENTRY", 30.0f);
    Connect("APRON_ENTRY", "APRON_GATEWAY", 28.0f);
    Connect("APRON_GATEWAY", "APRON_SIDE_FAR", 24.0f);
    Connect("APRON_SIDE_FAR", "APRON_SIDE_MIDDLE", 22.0f);
    Connect("APRON_SIDE_MIDDLE", "APRON_SIDE_NEAR", 22.0f);
    Connect("APRON_SIDE_NEAR", "APRON_BACK_ROW", 20.0f);

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
        // Runway exits are straight joins into the runway; keeping their
        // existing white edge markings visible matters more than capping them.
        bool isTurn = false;
        if (node.connectedNodeIds.size() == 2)
        {
            const TaxiwayNode* first = FindNode(node.connectedNodeIds[0]);
            const TaxiwayNode* second = FindNode(node.connectedNodeIds[1]);
            if (first != nullptr && second != nullptr)
            {
                const bool firstHorizontal = std::fabs(first->position.x - node.position.x) > 0.01f;
                const bool secondHorizontal = std::fabs(second->position.x - node.position.x) > 0.01f;
                isTurn = firstHorizontal != secondHorizontal;
            }
        }
        if (node.connectedNodeIds.size() < 3 && !isTurn) continue;

        float widestLane = 0.0f;
        for (const TaxiwayEdge& edge : edges)
            if (edge.fromNodeId == node.id || edge.toNodeId == node.id)
                widestLane = std::max(widestLane, edge.width);
        if (widestLane <= 0.0f) continue;

        DrawCylinder({node.position.x, 0.0f, node.position.z},
            (widestLane + 8.0f) * 0.72f, (widestLane + 8.0f) * 0.72f,
            0.11f, 20, TaxiwayShoulder);
        DrawCylinder({node.position.x, 0.004f, node.position.z},
            widestLane * 0.72f, widestLane * 0.72f, 0.11f, 20, TaxiwaySurface);
    }

    // Paint continuous centerlines after the intersection pads so they remain
    // unbroken through every join. Runway exits stop at the holding position.
    for (const TaxiwayEdge& edge : edges)
    {
        if (!edge.drawSurface) continue;
        const TaxiwayNode* from = FindNode(edge.fromNodeId);
        const TaxiwayNode* to = FindNode(edge.toNodeId);
        if (from == nullptr || to == nullptr) continue;
        Vector3 lineFrom = from->position;
        Vector3 lineTo = to->position;
        DrawTaxiwayCenterline(lineFrom, lineTo, edge.width, edge.stopAtHoldLine, edge.holdLineZ);
    }

    // Simplified runway holding-position marking. The solid pair faces the
    // taxiway approach; dashed bars sit beyond it on the runway side.
    const auto drawHoldMarking = [](float x, float solidOne, float solidTwo,
        float dashedOne, float dashedTwo)
    {
        constexpr float lineY = 0.135f;
        constexpr float lineThickness = 0.025f;
        constexpr float lineDepth = 0.5f;
        constexpr Color HoldMarking{245, 197, 55, 255};
        DrawCubeV({x, lineY, solidOne}, {30.0f, lineThickness, lineDepth}, HoldMarking);
        DrawCubeV({x, lineY, solidTwo}, {30.0f, lineThickness, lineDepth}, HoldMarking);

        for (float z : {dashedOne, dashedTwo})
        {
            for (int dash = 0; dash < 7; ++dash)
            {
                const float dashX = x - 12.0f + dash * 4.0f;
                DrawCubeV({dashX, lineY, z}, {2.0f, lineThickness, lineDepth}, HoldMarking);
            }
        }
    };

    const auto drawRunwayHold = [&drawHoldMarking](float x, float runwayEdgeZ, float taxiSide)
    {
        drawHoldMarking(x,
            runwayEdgeZ + taxiSide * 8.0f,
            runwayEdgeZ + taxiSide * 10.5f,
            runwayEdgeZ + taxiSide * 5.5f,
            runwayEdgeZ + taxiSide * 3.0f);
    };
    const float runwayExitXs[] = {-260.0f, 0.0f, 260.0f};
    for (float x : runwayExitXs)
    {
        drawRunwayHold(x, runways[0].center.z + runways[0].width * 0.5f, 1.0f);
        drawRunwayHold(x, runways[0].center.z - runways[0].width * 0.5f, -1.0f);
        drawRunwayHold(x, runways[1].center.z + runways[1].width * 0.5f, 1.0f);
        drawRunwayHold(x, runways[1].center.z - runways[1].width * 0.5f, -1.0f);
        drawRunwayHold(x, runways[2].center.z + runways[2].width * 0.5f, 1.0f);
    }
}
