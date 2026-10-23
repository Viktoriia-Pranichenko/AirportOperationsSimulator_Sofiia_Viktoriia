#include "airport_ground.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <unordered_map>
#include <cstddef>
#include <initializer_list>

namespace
{
    constexpr Color RunwayShoulder{79, 86, 89, 255};
    constexpr Color RunwaySurface{48, 55, 60, 255};
    constexpr Color TaxiwayShoulder{86, 94, 95, 255};
    constexpr Color TaxiwaySurface{75, 83, 85, 255};
    constexpr Color TaxiwayCenterline{241, 195, 70, 255};
    constexpr Color RunwayMarking{235, 236, 226, 255};
    constexpr float SharedTaxiwayLanePenalty = 10000.0f;

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

    void DrawRunwayDigit(char digit, float centerX, float centerZ)
    {
        // Simple seven-segment glyphs drawn directly onto the runway surface.
        constexpr float halfHeight = 5.0f;
        constexpr float halfWidth = 2.6f;
        constexpr float lineWidth = 1.3f;
        constexpr float paintY = 0.15f;
        const Vector3 topLeft{centerX - halfHeight, 0.0f, centerZ - halfWidth};
        const Vector3 topRight{centerX - halfHeight, 0.0f, centerZ + halfWidth};
        const Vector3 middleLeft{centerX, 0.0f, centerZ - halfWidth};
        const Vector3 middleRight{centerX, 0.0f, centerZ + halfWidth};
        const Vector3 bottomLeft{centerX + halfHeight, 0.0f, centerZ - halfWidth};
        const Vector3 bottomRight{centerX + halfHeight, 0.0f, centerZ + halfWidth};

        bool segments[7]{}; // top, upper-right, lower-right, bottom, lower-left, upper-left, middle
        switch (digit)
        {
            case '0': segments[0] = segments[1] = segments[2] = segments[3] = segments[4] = segments[5] = true; break;
            case '2': segments[0] = segments[1] = segments[6] = segments[4] = segments[3] = true; break;
            case '7': segments[0] = segments[1] = segments[2] = true; break;
            case '9': segments[0] = segments[1] = segments[2] = segments[3] = segments[5] = segments[6] = true; break;
            default: return;
        }

        const auto stroke = [paintY, lineWidth](Vector3 from, Vector3 to)
        {
            DrawFlatSegment(from, to, lineWidth, paintY, RunwayMarking);
        };
        if (segments[0]) stroke(topLeft, topRight);
        if (segments[1]) stroke(topRight, middleRight);
        if (segments[2]) stroke(middleRight, bottomRight);
        if (segments[3]) stroke(bottomLeft, bottomRight);
        if (segments[4]) stroke(middleLeft, bottomLeft);
        if (segments[5]) stroke(topLeft, middleLeft);
        if (segments[6]) stroke(middleLeft, middleRight);
    }

    void DrawRunwayNumber(const std::string& number, float centerX, float centerZ)
    {
        if (number.size() != 2) return;
        constexpr float digitSpacing = 6.2f;
        DrawRunwayDigit(number[0], centerX, centerZ - digitSpacing * 0.5f);
        DrawRunwayDigit(number[1], centerX, centerZ + digitSpacing * 0.5f);
    }

    void DrawGroundAroundApron(Color grassColor)
    {
        // Keep grass outside the connected apron and service-spur footprint.
        constexpr float groundY = -0.01f;
        constexpr float groundMinX = -750.0f;
        constexpr float groundMaxX = 750.0f;
        constexpr float groundMinZ = -810.0f;
        constexpr float groundMaxZ = 155.0f;
        constexpr float platformMinX = -220.0f;
        constexpr float platformMaxX = 220.0f;
        constexpr float spurMaxX = 280.0f;
        constexpr float platformMinZ = -270.0f;
        constexpr float platformMaxZ = 160.0f;
        constexpr float spurMinZ = -180.0f;
        constexpr float spurMaxZ = 45.0f;

        const auto drawGrassRect = [groundY, grassColor](
            float minX, float maxX, float minZ, float maxZ)
        {
            if (maxX <= minX || maxZ <= minZ) return;
            DrawPlane({(minX + maxX) * 0.5f, groundY, (minZ + maxZ) * 0.5f},
                {maxX - minX, maxZ - minZ}, grassColor);
        };

        drawGrassRect(groundMinX, platformMinX, groundMinZ, groundMaxZ);
        drawGrassRect(spurMaxX, groundMaxX, groundMinZ, groundMaxZ);
        drawGrassRect(platformMinX, platformMaxX, groundMinZ, platformMinZ);
        drawGrassRect(platformMaxX, spurMaxX, groundMinZ, spurMinZ);
        drawGrassRect(platformMaxX, spurMaxX, spurMaxZ, groundMaxZ);
    }

    void DrawRunway(const AirportRunway& runway)
    {
        const float centerX = runway.center.x;
        const float centerZ = runway.center.z;
        DrawCubeV({centerX, -0.015f, centerZ},
            {runway.length, 0.09f, runway.width}, RunwayShoulder);
        DrawCubeV({centerX, 0.04f, centerZ},
            {runway.length, 0.12f, runway.width}, RunwaySurface);

        const float edgeX = runway.length * 0.5f - 6.0f;
        const float northEdge = centerZ - runway.width * 0.5f + 2.0f;
        const float southEdge = centerZ + runway.width * 0.5f - 2.0f;
        DrawFlatSegment({centerX - edgeX, 0.0f, northEdge},
            {centerX + edgeX, 0.0f, northEdge}, 0.9f, 0.15f, RunwayMarking);
        DrawFlatSegment({centerX - edgeX, 0.0f, southEdge},
            {centerX + edgeX, 0.0f, southEdge}, 0.9f, 0.15f, RunwayMarking);

        const float halfLength = runway.length * 0.5f;
        const float aimingPointDistance = halfLength - 50.0f;
        for (float offset = -halfLength + 45.0f;
             offset + 12.0f <= halfLength - 45.0f; offset += 30.0f)
        {
            const float dashCenter = offset + 6.0f;
            if (std::fabs(dashCenter + aimingPointDistance) < 22.0f ||
                std::fabs(dashCenter - aimingPointDistance) < 22.0f) continue;
            DrawFlatSegment({centerX + offset, 0.0f, centerZ},
                {centerX + offset + 12.0f, 0.0f, centerZ}, 0.9f, 0.15f, RunwayMarking);
        }

        // Eight transverse threshold stripes at each runway end.
        for (int stripe = 0; stripe < 8; ++stripe)
        {
            const float z = centerZ + (static_cast<float>(stripe) - 3.5f) * 2.45f;
            const float westX = centerX - halfLength + 8.0f;
            const float eastX = centerX + halfLength - 8.0f;
            DrawFlatSegment({westX, 0.0f, z - 0.85f}, {westX, 0.0f, z + 0.85f},
                1.5f, 0.15f, RunwayMarking);
            DrawFlatSegment({eastX, 0.0f, z - 0.85f}, {eastX, 0.0f, z + 0.85f},
                1.5f, 0.15f, RunwayMarking);
        }

        // Mark both runway directions just inside their threshold stripes.
        DrawRunwayNumber(runway.westEndNumber, centerX - halfLength + 25.0f, centerZ);
        DrawRunwayNumber(runway.eastEndNumber, centerX + halfLength - 25.0f, centerZ);

        // Aiming-point blocks sit on either side of the dashed centreline.
        for (int side : {-1, 1})
        {
            const float x = centerX + side * aimingPointDistance;
            for (int zSide : {-1, 1})
                DrawFlatSegment({x - 10.0f, 0.0f, centerZ + zSide * 5.0f},
                    {x + 10.0f, 0.0f, centerZ + zSide * 5.0f},
                    1.8f, 0.15f, RunwayMarking);
        }
    }

    void DrawTaxiwaySurface(Vector3 from, Vector3 to, float width)
    {
        DrawFlatSegment(from, to, width + 4.0f, 0.028f, TaxiwayShoulder);
        DrawFlatSegment(from, to, width, 0.045f, TaxiwaySurface);
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

        DrawFlatSegment(from, to, 0.75f, 0.12f, TaxiwayCenterline);
    }

}

AirportGround::AirportGround(const AirportTerminal& terminal)
{
    // Three shorter parallel runways keep a visible grass buffer between
    // each runway and the inter-runway taxi lanes.
    runways = {
        {"RUNWAY_A", {0.0f, 0.0f, -507.0f}, 450.0f, 26.0f, "09", "27"},
        {"RUNWAY_B", {0.0f, 0.0f, -642.0f}, 450.0f, 26.0f, "09", "27"},
        {"RUNWAY_C", {0.0f, 0.0f, -777.0f}, 450.0f, 26.0f, "09", "27"}
    };

    constexpr float routeY = 0.08f;
    // Place runway connectors close to each threshold, with enough paved
    // distance for a curved entry/exit before the runway end.
    constexpr float runwayExitXs[] = {-175.0f, 0.0f, 175.0f};
    constexpr float taxiwayExitXs[] = {-145.0f, 0.0f, 145.0f};
    constexpr char runwaySuffixes[] = {'W', 'C', 'E'};
    const std::string runwayPrefixes[] = {"RUNWAY_A", "RUNWAY_B", "RUNWAY_C"};

    // Runway exits lie away from the centre and meet the parallel taxiways via
    // curved, angled connectors. Keep the central graph node for runway routing.
    for (std::size_t r = 0; r < runways.size(); ++r)
    {
        const float z = runways[r].center.z;
        const float halfLength = runways[r].length * 0.5f - 10.0f;
        const std::string prefix = r == 0 ? std::string("RUNWAY") : runwayPrefixes[r];
        const std::string westEnd = prefix + "_START_W";
        const std::string eastEnd = prefix + "_END_E";
        const std::string approach = "APPROACH_" + runwayPrefixes[r].substr(7) + "_W";
        const std::string eastApproach = "APPROACH_" + runwayPrefixes[r].substr(7) + "_E";
        AddNode(approach, {-halfLength - 50.0f, routeY, z});
        AddNode(eastApproach, {halfLength + 50.0f, routeY, z});
        AddNode(westEnd, {-halfLength, routeY, z});
        AddNode(prefix + "_W", {runwayExitXs[0], routeY, z});
        AddNode(prefix + "_CENTER", {runwayExitXs[1], routeY, z});
        AddNode(prefix + "_E", {runwayExitXs[2], routeY, z});
        AddNode(eastEnd, {halfLength, routeY, z});
        Connect(westEnd, prefix + "_W", 1.0f, false);
        Connect(approach, westEnd, 1.0f, false);
        Connect(prefix + "_W", prefix + "_CENTER", 1.0f, false);
        Connect(prefix + "_CENTER", prefix + "_E", 1.0f, false);
        Connect(prefix + "_E", eastEnd, 1.0f, false);
        Connect(eastEnd, eastApproach, 1.0f, false);
    }

    // Taxiway A is the primary parallel taxiway between Runway A and apron.
    constexpr float parallelTaxiZ = -425.0f;
    AddNode("A_WEST", {-190.0f, routeY, parallelTaxiZ});
    AddNode("A_EXIT_W", {taxiwayExitXs[0], routeY, parallelTaxiZ});
    AddNode("A_CENTER", {0.0f, routeY, parallelTaxiZ});
    AddNode("A_EXIT_E", {taxiwayExitXs[2], routeY, parallelTaxiZ});
    AddNode("A_EAST", {190.0f, routeY, parallelTaxiZ});
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
        constexpr int segments = 12;
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

    const auto connectAngledExit = [this, &makeBezierConnection, routeY](
        const std::string& pathId, const std::string& fromId,
        const std::string& toId, float inwardDirection, float width)
    {
        const TaxiwayNode* fromNode = FindNode(fromId);
        const TaxiwayNode* toNode = FindNode(toId);
        if (fromNode == nullptr || toNode == nullptr) return;
        const Vector3 start = fromNode->position;
        const Vector3 finish = toNode->position;
        const float zDirection = finish.z >= start.z ? 1.0f : -1.0f;
        makeBezierConnection(pathId, fromId, toId,
            {start.x + inwardDirection * 20.0f, routeY, start.z},
            {finish.x, routeY, finish.z - zDirection * 18.0f}, width);
    };

    // Shared, parallel taxiways in the grass-separated runway corridors.
    for (int pair = 0; pair < 2; ++pair)
    {
        const std::string prefix = pairPrefixes[pair];
        const float taxiZ = pairTaxiZs[pair];
        const std::string west = prefix + "_TAXI_W";
        const std::string exitWest = prefix + "_EXIT_W";
        const std::string lowerExitWest = prefix + "_EXIT_W_LOWER";
        const std::string center = prefix + "_CENTER";
        const std::string lowerExitEast = prefix + "_EXIT_E_LOWER";
        const std::string exitEast = prefix + "_EXIT_E";
        const std::string east = prefix + "_TAXI_E";
        AddNode(west, {-190.0f, routeY, taxiZ});
        AddNode(exitWest, {taxiwayExitXs[0], routeY, taxiZ});
        AddNode(lowerExitWest, {taxiwayExitXs[0] + 30.0f, routeY, taxiZ});
        AddNode(center, {0.0f, routeY, taxiZ});
        AddNode(lowerExitEast, {taxiwayExitXs[2] - 30.0f, routeY, taxiZ});
        AddNode(exitEast, {taxiwayExitXs[2], routeY, taxiZ});
        AddNode(east, {190.0f, routeY, taxiZ});
        Connect(west, exitWest, 12.0f);
        Connect(exitWest, lowerExitWest, 12.0f);
        Connect(lowerExitWest, center, 12.0f);
        Connect(center, lowerExitEast, 12.0f);
        Connect(lowerExitEast, exitEast, 12.0f);
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
            if (i != 1)
            {
                const std::string upperTaxiNode = i == 0 ? exitWest : exitEast;
                const std::string lowerTaxiNode = i == 0 ? lowerExitWest : lowerExitEast;
                const float inwardDirection = i == 0 ? 1.0f : -1.0f;
                const std::string upperHold = prefix + "_UPPER_HOLD_" + runwaySuffixes[i];
                const std::string lowerHold = prefix + "_LOWER_HOLD_" + runwaySuffixes[i];
                AddNode(upperHold, {runwayExitXs[i], routeY, upperEdge + 6.0f});
                AddNode(lowerHold, {runwayExitXs[i], routeY, lowerEdge - 6.0f});
                Connect(upperExit, upperHold, 12.0f);
                Connect(lowerExit, lowerHold, 12.0f);
                connectAngledExit(prefix + "_UPPER_TURN_" + runwaySuffixes[i],
                    upperHold, upperTaxiNode, inwardDirection, 12.0f);
                connectAngledExit(prefix + "_LOWER_TURN_" + runwaySuffixes[i],
                    lowerHold, lowerTaxiNode, inwardDirection, 12.0f);
            }
        }
    }

    // Runway A exits to its south-side parallel taxiway.
    const char* runwayAExits[] = {"RWY_EXIT_W", "RWY_EXIT_C", "RWY_EXIT_E"};
    const char* runwayAPathNodes[] = {"RUNWAY_W", "RUNWAY_CENTER", "RUNWAY_E"};
    const char* parallelNodes[] = {"A_EXIT_W", "A_CENTER", "A_EXIT_E"};
    for (int i = 0; i < 3; ++i)
    {
        if (i == 1) continue;
        AddNode(runwayAExits[i], {runwayExitXs[i], routeY, runwaySouthEdges[0]});
        Connect(runwayAPathNodes[i], runwayAExits[i], 1.0f, false);
        const std::string holdNode = std::string("A_HOLD_") + runwaySuffixes[i];
        AddNode(holdNode, {runwayExitXs[i], routeY, runwaySouthEdges[0] + 6.0f});
        Connect(runwayAExits[i], holdNode, 12.0f);
        connectAngledExit(std::string("RUNWAY_A_TURN_") + runwaySuffixes[i],
            holdNode, parallelNodes[i], i == 0 ? 1.0f : -1.0f, 13.0f);
    }

    // The eastern bypass gives traffic a route between the parallel taxiways.
    AddNode("BYPASS_A", {230.0f, routeY, parallelTaxiZ});
    AddNode("BYPASS_AB", {230.0f, routeY, pairTaxiZs[0]});
    AddNode("BYPASS_BC", {230.0f, routeY, runways[2].center.z});
    AddNode("WEST_BYPASS_A", {-230.0f, routeY, parallelTaxiZ});
    AddNode("WEST_BYPASS_AB", {-230.0f, routeY, pairTaxiZs[0]});
    AddNode("WEST_BYPASS_BC", {-230.0f, routeY, runways[2].center.z});
    Connect("A_EAST", "BYPASS_A", 12.0f);
    Connect("BYPASS_A", "BYPASS_AB", 11.0f);
    Connect("BYPASS_AB", "BYPASS_BC", 11.0f);
    Connect("A_WEST", "WEST_BYPASS_A", 12.0f);
    Connect("WEST_BYPASS_A", "WEST_BYPASS_AB", 11.0f);
    Connect("WEST_BYPASS_AB", "WEST_BYPASS_BC", 11.0f);
    makeBezierConnection("BYPASS_AB_TURN", "BYPASS_AB", "AB_TAXI_E",
        {230.0f, routeY, pairTaxiZs[0] + 17.5f},
        {210.0f, routeY, pairTaxiZs[0]}, 11.0f);
    makeBezierConnection("BYPASS_BC_TURN", "BYPASS_BC", "BC_TAXI_E",
        {230.0f, routeY, runways[2].center.z + 20.0f},
        {210.0f, routeY, pairTaxiZs[1]}, 11.0f);
    makeBezierConnection("WEST_BYPASS_AB_TURN", "WEST_BYPASS_AB", "AB_TAXI_W",
        {-230.0f, routeY, pairTaxiZs[0] + 17.5f},
        {-210.0f, routeY, pairTaxiZs[0]}, 11.0f);
    makeBezierConnection("WEST_BYPASS_BC_TURN", "WEST_BYPASS_BC", "BC_TAXI_W",
        {-230.0f, routeY, runways[2].center.z + 20.0f},
        {-210.0f, routeY, pairTaxiZs[1]}, 11.0f);

    // Preserve the approach up to the apron edge. Hide only the dark-grey
    // straight taxiway pieces inside the red-marked apron section. The graph
    // stays continuous across these visual gaps for arrival/departure routing.
    AddNode("APRON_ENTRY", {0.0f, routeY, -220.0f});
    AddNode("MAIN_TAXI_W_VISUAL_GAP_END", {-25.0f, routeY, -270.0f});
    AddNode("MAIN_TAXI_W_TOP", {-25.0f, routeY, -220.0f});
    AddNode("MAIN_TAXI_W_BOTTOM", {-25.0f, routeY, parallelTaxiZ});
    AddNode("MAIN_TAXI_E_VISUAL_GAP_END", {25.0f, routeY, -270.0f});
    AddNode("MAIN_TAXI_E_TOP", {25.0f, routeY, -220.0f});
    AddNode("MAIN_TAXI_E_BOTTOM", {25.0f, routeY, parallelTaxiZ});
    AddNode("MAIN_TAXI_W_GATE", {-25.0f, routeY, -60.0f});
    AddNode("MAIN_TAXI_E_GATE", {25.0f, routeY, -60.0f});
    AddNode("APRON_GATEWAY", {0.0f, routeY, -190.0f});
    AddNode("APRON_SIDE_FAR", {0.0f, routeY, -180.0f});
    AddNode("WING_SPINE_3", {0.0f, routeY, -174.0f});
    AddNode("APRON_SIDE_MIDDLE", {0.0f, routeY, -140.0f});
    AddNode("WING_SPINE_2", {0.0f, routeY, -134.0f});
    AddNode("APRON_SIDE_NEAR", {0.0f, routeY, -100.0f});
    AddNode("WING_SPINE_1", {0.0f, routeY, -94.0f});
    AddNode("APRON_BACK_ROW", {0.0f, routeY, -60.0f});
    Connect("APRON_ENTRY", "MAIN_TAXI_W_TOP", 11.0f, false);
    Connect("MAIN_TAXI_W_TOP", "MAIN_TAXI_W_VISUAL_GAP_END", 11.0f, false);
    Connect("MAIN_TAXI_W_VISUAL_GAP_END", "MAIN_TAXI_W_BOTTOM", 11.0f);
    Connect("MAIN_TAXI_W_BOTTOM", "A_EXIT_W", 11.0f);
    Connect("APRON_GATEWAY", "MAIN_TAXI_E_TOP", 11.0f, false);
    Connect("MAIN_TAXI_E_TOP", "MAIN_TAXI_E_VISUAL_GAP_END", 11.0f, false);
    Connect("MAIN_TAXI_E_VISUAL_GAP_END", "MAIN_TAXI_E_BOTTOM", 11.0f);
    Connect("MAIN_TAXI_E_BOTTOM", "A_EXIT_E", 11.0f);
    Connect("APRON_ENTRY", "APRON_GATEWAY", 14.0f, false);
    Connect("APRON_GATEWAY", "APRON_SIDE_FAR", 12.0f, false);
    Connect("APRON_SIDE_FAR", "WING_SPINE_3", 11.0f, false);
    Connect("WING_SPINE_3", "APRON_SIDE_MIDDLE", 11.0f, false);
    Connect("APRON_SIDE_MIDDLE", "WING_SPINE_2", 11.0f, false);
    Connect("WING_SPINE_2", "APRON_SIDE_NEAR", 11.0f, false);
    Connect("APRON_SIDE_NEAR", "WING_SPINE_1", 10.0f, false);
    Connect("WING_SPINE_1", "APRON_BACK_ROW", 10.0f, false);

    // A separate gate-side apron lane gives all four facade stands an approach.
    constexpr float gateXs[] = {-60.0f, -20.0f, 20.0f, 60.0f};
    AddNode("GATE_ROW_W", {-75.0f, routeY, -60.0f});
    AddNode("GATE_ROW_1", {gateXs[0], routeY, -60.0f});
    AddNode("GATE_ROW_2", {gateXs[1], routeY, -60.0f});
    AddNode("GATE_ROW_3", {gateXs[2], routeY, -60.0f});
    AddNode("GATE_ROW_4", {gateXs[3], routeY, -60.0f});
    AddNode("GATE_ROW_E", {75.0f, routeY, -60.0f});
    // Preserve this straight gate-row routing spine for aircraft pathfinding,
    // while hiding its pavement and feeder strips from the apron rendering.
    Connect("MAIN_TAXI_W_GATE", "GATE_ROW_2", 10.0f, false);
    Connect("MAIN_TAXI_E_GATE", "GATE_ROW_3", 10.0f, false);
    Connect("GATE_ROW_W", "GATE_ROW_1", 10.0f, false);
    Connect("GATE_ROW_1", "GATE_ROW_2", 10.0f, false);
    Connect("GATE_ROW_2", "APRON_BACK_ROW", 10.0f, false);
    Connect("APRON_BACK_ROW", "GATE_ROW_3", 10.0f, false);
    Connect("GATE_ROW_3", "GATE_ROW_4", 10.0f, false);
    Connect("GATE_ROW_4", "GATE_ROW_E", 10.0f, false);

    // Taxiway branches reach the six wing stands without crossing the stands.
    const float wingTaxiZs[] = {-94.0f, -134.0f, -174.0f};
    for (int row = 0; row < 3; ++row)
    {
        const float z = wingTaxiZs[row];
        const std::string rowId = "WING_ROW_" + std::to_string(row + 1);
        const std::string westId = rowId + "_W";
        const std::string eastId = rowId + "_E";
        AddNode(westId, {-25.0f, routeY, z});
        AddNode(eastId, {25.0f, routeY, z});
        const std::string spineId = row == 0 ? "WING_SPINE_1" :
            row == 1 ? "WING_SPINE_2" : "WING_SPINE_3";
        Connect(spineId, westId, 9.0f, false);
        Connect(spineId, eastId, 9.0f, false);
        Connect(westId, eastId, 9.0f, false);
    }

    // Extend both terminal connectors directly into the stand apron. The
    // west lane feeds west stands and arrivals; the east lane feeds east-side
    // stands and departures. The existing central spine remains as a cross-link.
    // Keep these direct terminal-side links in the waypoint graph, but hide
    // their straight pavement so the apron remains visually clear.
    Connect("MAIN_TAXI_W_TOP", "WING_ROW_3_W", 11.0f, false);
    Connect("WING_ROW_3_W", "WING_ROW_2_W", 10.0f, false);
    Connect("WING_ROW_2_W", "WING_ROW_1_W", 10.0f, false);
    Connect("WING_ROW_1_W", "MAIN_TAXI_W_GATE", 10.0f, false);
    Connect("MAIN_TAXI_E_TOP", "WING_ROW_3_E", 11.0f, false);
    Connect("WING_ROW_3_E", "WING_ROW_2_E", 10.0f, false);
    Connect("WING_ROW_2_E", "WING_ROW_1_E", 10.0f, false);
    Connect("WING_ROW_1_E", "MAIN_TAXI_E_GATE", 10.0f, false);

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
            Connect("GATE_ROW_" + rowIndex, entryId, 9.0f, false);
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
            Connect(rowId, entryId, 9.0f, false);
            Connect(entryId, stand.id, 1.0f, false);
        }
    }

    for (const TaxiwayNode& node : nodes)
    {
        int pavementNeighborCount = 0;
        Vector3 firstNeighbor{};
        Vector3 secondNeighbor{};
        float widestLane = 0.0f;
        for (const std::size_t edgeIndex : node.connectedEdgeIndices)
        {
            const TaxiwayEdge& edge = edges[edgeIndex];
            if (!edge.drawSurface) continue;
            const Vector3 neighborPosition = edge.fromNodeId == node.id ?
                edge.toPosition : edge.fromPosition;
            if (pavementNeighborCount == 0) firstNeighbor = neighborPosition;
            else if (pavementNeighborCount == 1) secondNeighbor = neighborPosition;
            ++pavementNeighborCount;
            widestLane = std::max(widestLane, edge.width);
        }
        if (pavementNeighborCount < 2 || widestLane <= 0.0f) continue;

        bool needsJoinPad = pavementNeighborCount >= 3;
        if (pavementNeighborCount == 2)
        {
            const float ax = firstNeighbor.x - node.position.x;
            const float az = firstNeighbor.z - node.position.z;
            const float bx = secondNeighbor.x - node.position.x;
            const float bz = secondNeighbor.z - node.position.z;
            const float aLength = std::sqrt(ax * ax + az * az);
            const float bLength = std::sqrt(bx * bx + bz * bz);
            if (aLength > 0.01f && bLength > 0.01f)
                needsJoinPad = (ax * bx + az * bz) / (aLength * bLength) > -0.85f;
        }
        if (needsJoinPad) taxiwayJunctions.push_back({node.position, widestLane});
    }
}

void AirportGround::AddNode(const std::string& id, Vector3 position)
{
    const std::size_t index = nodes.size();
    nodes.push_back({id, position, {}, {}, false});
    nodeIndexById[id] = index;
}

void AirportGround::Connect(const std::string& from, const std::string& to, float width,
    bool drawSurface, bool stopAtHoldLine, float holdLineZ)
{
    const auto fromIndex = nodeIndexById.find(from);
    const auto toIndex = nodeIndexById.find(to);
    if (fromIndex == nodeIndexById.end() || toIndex == nodeIndexById.end()) return;

    TaxiwayNode& fromNode = nodes[fromIndex->second];
    TaxiwayNode& toNode = nodes[toIndex->second];
    const std::size_t edgeIndex = edges.size();
    edges.push_back({from, to, width, drawSurface, stopAtHoldLine,
        holdLineZ, true, fromNode.position, toNode.position});
    fromNode.connectedNodeIds.push_back(to);
    fromNode.connectedEdgeIndices.push_back(edgeIndex);
    toNode.connectedNodeIds.push_back(from);
    toNode.connectedEdgeIndices.push_back(edgeIndex);
}

const TaxiwayNode* AirportGround::FindNode(const std::string& id) const
{
    const auto found = nodeIndexById.find(id);
    return found == nodeIndexById.end() ? nullptr : &nodes[found->second];
}

bool AirportGround::SetTaxiwaySectionOpen(const std::string& fromNodeId,
    const std::string& toNodeId, bool open)
{
    for (TaxiwayEdge& edge : edges)
    {
        if ((edge.fromNodeId == fromNodeId && edge.toNodeId == toNodeId) ||
            (edge.fromNodeId == toNodeId && edge.toNodeId == fromNodeId))
        {
            edge.open = open;
            return true;
        }
    }
    return false;
}

bool AirportGround::SetNodeBlocked(const std::string& nodeId, bool blocked)
{
    const auto found = std::find_if(nodes.begin(), nodes.end(),
        [&nodeId](const TaxiwayNode& node) { return node.id == nodeId; });
    if (found == nodes.end()) return false;
    found->blockedByObstacle = blocked;
    return true;
}

bool AirportGround::SetRunwayAvailable(const std::string& runwayId, bool available)
{
    const auto found = std::find_if(runways.begin(), runways.end(),
        [&runwayId](const AirportRunway& runway) { return runway.id == runwayId; });
    if (found == runways.end()) return false;
    found->available = available;
    return true;
}

bool AirportGround::IsRunwayOrApproachNode(const std::string& id) const
{
    if (id.rfind("APPROACH_", 0) == 0) return true;
    for (const char* prefix : {"RUNWAY", "RUNWAY_A", "RUNWAY_B", "RUNWAY_C"})
    {
        const std::string name(prefix);
        if (id == name + "_START_W" || id == name + "_W" ||
            id == name + "_CENTER" || id == name + "_E" ||
            id == name + "_END_E")
            return true;
    }
    return false;
}

std::vector<std::string> AirportGround::FindRouteNodeIds(
    const std::string& startNodeId, const std::string& destinationNodeId,
    TaxiRoutePurpose purpose) const
{
    const TaxiwayNode* start = FindNode(startNodeId);
    const TaxiwayNode* destination = FindNode(destinationNodeId);
    if (start == nullptr || destination == nullptr ||
        start->blockedByObstacle || destination->blockedByObstacle)
        return {};
    if (startNodeId == destinationNodeId) return {startNodeId};

    // Weighted shortest path follows actual pavement length. A lane preference
    // then keeps inbound and outbound traffic on opposite sides when both are
    // available, without making the unused side unreachable.
    using QueueEntry = std::pair<float, std::string>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> frontier;
    std::unordered_map<std::string, std::string> predecessor;
    std::unordered_map<std::string, float> distance;
    frontier.push({0.0f, startNodeId});
    distance.emplace(startNodeId, 0.0f);
    predecessor.emplace(startNodeId, "");

    while (!frontier.empty())
    {
        const QueueEntry entry = frontier.top();
        frontier.pop();
        const float currentDistance = entry.first;
        const std::string& currentId = entry.second;
        const auto bestDistance = distance.find(currentId);
        if (bestDistance == distance.end() || currentDistance > bestDistance->second) continue;
        if (currentId == destinationNodeId) break;
        const TaxiwayNode* current = FindNode(currentId);
        if (current == nullptr) continue;
        for (std::size_t neighborIndex = 0;
            neighborIndex < current->connectedNodeIds.size(); ++neighborIndex)
        {
            const std::string& neighborId = current->connectedNodeIds[neighborIndex];
            const TaxiwayNode* neighbor = FindNode(neighborId);
            if (neighbor == nullptr || neighbor->blockedByObstacle) continue;
            // Shared detours stay on taxiways; runway nodes may only be route endpoints.
            if (purpose == TaxiRoutePurpose::SharedTaxiway &&
                ((IsRunwayOrApproachNode(currentId) && currentId != startNodeId) ||
                    (IsRunwayOrApproachNode(neighborId) &&
                        neighborId != destinationNodeId) ||
                    (IsRunwayOrApproachNode(currentId) &&
                        IsRunwayOrApproachNode(neighborId))))
                continue;
            const std::size_t edgeIndex = current->connectedEdgeIndices[neighborIndex];
            if (!edges[edgeIndex].open) continue;
            const float dx = neighbor->position.x - current->position.x;
            const float dz = neighbor->position.z - current->position.z;
            float edgeCost = std::sqrt(dx * dx + dz * dz);
            const auto isWestLane = [](const std::string& id)
            {
                return id.find("MAIN_TAXI_W") != std::string::npos ||
                    id == "A_WEST" || id == "A_EXIT_W" ||
                    id.find("_TAXI_W") != std::string::npos ||
                    id.find("_EXIT_W") != std::string::npos ||
                    id.find("WEST_BYPASS") != std::string::npos;
            };
            const auto isEastLane = [](const std::string& id)
            {
                return id.find("MAIN_TAXI_E") != std::string::npos ||
                    id == "A_EAST" || id == "A_EXIT_E" ||
                    id.find("_TAXI_E") != std::string::npos ||
                    id.find("_EXIT_E") != std::string::npos ||
                    id.find("BYPASS_") == 0;
            };
            const bool usesWestLane = isWestLane(currentId) || isWestLane(neighborId);
            const bool usesEastLane = isEastLane(currentId) || isEastLane(neighborId);
            if (purpose == TaxiRoutePurpose::SharedTaxiway && usesWestLane)
                edgeCost += SharedTaxiwayLanePenalty;
            else
            {
                if (purpose == TaxiRoutePurpose::Arrival && usesEastLane) edgeCost += 280.0f;
                if (purpose == TaxiRoutePurpose::Departure && usesWestLane) edgeCost += 280.0f;
            }

            const float nextDistance = currentDistance + edgeCost;
            const auto oldDistance = distance.find(neighborId);
            if (oldDistance != distance.end() && nextDistance >= oldDistance->second) continue;
            distance[neighborId] = nextDistance;
            predecessor[neighborId] = currentId;
            frontier.push({nextDistance, neighborId});
        }
    }
    if (distance.find(destinationNodeId) != distance.end())
    {
        std::vector<std::string> route;
        for (std::string id = destinationNodeId; !id.empty(); id = predecessor.at(id))
            route.push_back(id);
        std::reverse(route.begin(), route.end());
        return route;
    }
    return {};
}

void AirportGround::Draw(Color grassColor) const
{
    // Ground remains outside the terminal slab to avoid coplanar flicker.
    DrawGroundAroundApron(grassColor);
    for (const AirportRunway& runway : runways)
        DrawRunway(runway);

    for (const TaxiwayEdge& edge : edges)
    {
        if (!edge.drawSurface) continue;
        DrawTaxiwaySurface(edge.fromPosition, edge.toPosition, edge.width);
    }

    for (const TaxiwayJunction& junction : taxiwayJunctions)
    {
        DrawCylinder({junction.position.x, -0.015f, junction.position.z},
            (junction.widestLane + 4.0f) * 0.5f,
            (junction.widestLane + 4.0f) * 0.5f,
            0.051f, 20, TaxiwayShoulder);
        DrawCylinder({junction.position.x, -0.02f, junction.position.z},
            junction.widestLane * 0.5f, junction.widestLane * 0.5f,
            0.073f, 20, TaxiwaySurface);
    }

    // Paint continuous centerlines after the intersection pads so they remain
    // unbroken through every join. Runway exits stop at the holding position.
    for (const TaxiwayEdge& edge : edges)
    {
        if (!edge.drawSurface) continue;
        DrawTaxiwayCenterline(edge.fromPosition, edge.toPosition,
            edge.stopAtHoldLine, edge.holdLineZ);
    }

    // Operational closures remain visible as a red transverse bar. Route
    // searches skip the same edge, so the visual closure and graph agree.
    for (const TaxiwayEdge& edge : edges)
    {
        if (edge.open) continue;
        const float dx = edge.toPosition.x - edge.fromPosition.x;
        const float dz = edge.toPosition.z - edge.fromPosition.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        if (length <= 0.01f) continue;
        const float ux = dx / length;
        const float uz = dz / length;
        const float px = -uz * edge.width * 0.5f;
        const float pz = ux * edge.width * 0.5f;
        const float barHalfLength = 1.25f;
        const Vector3 center{(edge.fromPosition.x + edge.toPosition.x) * 0.5f,
            0.19f, (edge.fromPosition.z + edge.toPosition.z) * 0.5f};
        DrawQuad({center.x - px - ux * barHalfLength, center.y, center.z - pz - uz * barHalfLength},
            {center.x + px - ux * barHalfLength, center.y, center.z + pz - uz * barHalfLength},
            {center.x + px + ux * barHalfLength, center.y, center.z + pz + uz * barHalfLength},
            {center.x - px + ux * barHalfLength, center.y, center.z - pz + uz * barHalfLength},
            Color{203, 65, 57, 255});
    }

    // Blocked route nodes show the obstacle that caused route searches to
    // avoid that intersection or waypoint.
    for (const TaxiwayNode& node : nodes)
    {
        if (node.blockedByObstacle)
            DrawCubeV({node.position.x, 0.7f, node.position.z},
                {4.0f, 1.2f, 4.0f}, Color{203, 65, 57, 255});
    }

    // Simplified runway holding-position marking. The solid pair faces the
    // taxiway approach; dashed bars sit beyond it on the runway side.
    const auto drawHoldMarking = [](float x, float solidOne, float solidTwo,
        float dashedOne, float dashedTwo)
    {
        constexpr float lineY = 0.17f;
        constexpr float lineThickness = 0.025f;
        constexpr float lineDepth = 0.4f;
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
    constexpr float runwayExitXs[] = {-175.0f, 0.0f, 175.0f};
    for (const AirportRunway& runway : runways)
    {
        for (int exitSide : {0, 2})
        {
            const float x = runwayExitXs[exitSide];
            drawRunwayHold(x, runway.center.z + runway.width * 0.5f, 1.0f);
            drawRunwayHold(x, runway.center.z - runway.width * 0.5f, -1.0f);
        }
    }
}
