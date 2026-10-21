#include "airport_ground.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
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

    void DrawRunwayDigit(char digit, float centerX, float centerZ)
    {
        // Simple seven-segment glyphs drawn directly onto the runway surface.
        constexpr float halfHeight = 5.0f;
        constexpr float halfWidth = 2.6f;
        constexpr float lineWidth = 1.15f;
        constexpr float paintY = 0.112f;
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

    void DrawGroundAroundApron()
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

        const auto drawGrassRect = [groundY](float minX, float maxX, float minZ, float maxZ)
        {
            if (maxX <= minX || maxZ <= minZ) return;
            DrawPlane({(minX + maxX) * 0.5f, groundY, (minZ + maxZ) * 0.5f},
                {maxX - minX, maxZ - minZ}, Grass);
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
            {centerX + edgeX, 0.0f, northEdge}, 0.55f, 0.108f, RunwayMarking);
        DrawFlatSegment({centerX - edgeX, 0.0f, southEdge},
            {centerX + edgeX, 0.0f, southEdge}, 0.55f, 0.108f, RunwayMarking);

        const float halfLength = runway.length * 0.5f;
        const float aimingPointDistance = halfLength - 50.0f;
        for (float offset = -halfLength + 45.0f;
             offset + 12.0f <= halfLength - 45.0f; offset += 30.0f)
        {
            const float dashCenter = offset + 6.0f;
            if (std::fabs(dashCenter + aimingPointDistance) < 22.0f ||
                std::fabs(dashCenter - aimingPointDistance) < 22.0f) continue;
            DrawFlatSegment({centerX + offset, 0.0f, centerZ},
                {centerX + offset + 12.0f, 0.0f, centerZ}, 0.55f, 0.109f, RunwayMarking);
        }

        // Eight transverse threshold stripes at each runway end.
        for (int stripe = 0; stripe < 8; ++stripe)
        {
            const float z = centerZ + (static_cast<float>(stripe) - 3.5f) * 2.45f;
            const float westX = centerX - halfLength + 8.0f;
            const float eastX = centerX + halfLength - 8.0f;
            DrawFlatSegment({westX, 0.0f, z - 0.85f}, {westX, 0.0f, z + 0.85f},
                1.2f, 0.111f, RunwayMarking);
            DrawFlatSegment({eastX, 0.0f, z - 0.85f}, {eastX, 0.0f, z + 0.85f},
                1.2f, 0.111f, RunwayMarking);
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
                    1.8f, 0.11f, RunwayMarking);
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
        {"RUNWAY_A", {0.0f, 0.0f, -507.0f}, 300.0f, 26.0f, "09", "27"},
        {"RUNWAY_B", {0.0f, 0.0f, -642.0f}, 300.0f, 26.0f, "09", "27"},
        {"RUNWAY_C", {0.0f, 0.0f, -777.0f}, 300.0f, 26.0f, "09", "27"}
    };

    constexpr float routeY = 0.08f;
    constexpr float runwayExitXs[] = {-85.0f, 0.0f, 85.0f};
    constexpr float taxiwayExitXs[] = {-65.0f, 0.0f, 65.0f};
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
        AddNode(approach, {-halfLength - 50.0f, routeY, z});
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
                connectAngledExit(prefix + "_UPPER_TURN_" + runwaySuffixes[i],
                    upperExit, upperTaxiNode, inwardDirection, 12.0f);
                connectAngledExit(prefix + "_LOWER_TURN_" + runwaySuffixes[i],
                    lowerExit, lowerTaxiNode, inwardDirection, 12.0f);
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
        connectAngledExit(std::string("RUNWAY_A_TURN_") + runwaySuffixes[i],
            runwayAExits[i], parallelNodes[i], i == 0 ? 1.0f : -1.0f, 13.0f);
    }

    // The eastern bypass gives traffic a route between the parallel taxiways.
    AddNode("BYPASS_A", {230.0f, routeY, parallelTaxiZ});
    AddNode("BYPASS_AB", {230.0f, routeY, pairTaxiZs[0]});
    AddNode("BYPASS_BC", {230.0f, routeY, pairTaxiZs[1]});
    Connect("A_EAST", "BYPASS_A", 12.0f);
    Connect("BYPASS_A", "BYPASS_AB", 11.0f);
    Connect("BYPASS_AB", "BYPASS_BC", 11.0f);
    makeBezierConnection("BYPASS_AB_TURN", "BYPASS_AB", "AB_TAXI_E",
        {230.0f, routeY, pairTaxiZs[0] + 17.5f},
        {210.0f, routeY, pairTaxiZs[0]}, 11.0f);
    makeBezierConnection("BYPASS_BC_TURN", "BYPASS_BC", "BC_TAXI_E",
        {230.0f, routeY, pairTaxiZs[1] + 17.5f},
        {210.0f, routeY, pairTaxiZs[1]}, 11.0f);

    // Preserve the approach up to the apron edge. Hide only the dark-grey
    // taxiway surfaces inside the marked apron network.
    AddNode("APRON_ENTRY", {0.0f, routeY, -220.0f});
    AddNode("APRON_GATEWAY", {0.0f, routeY, -190.0f});
    AddNode("APRON_SIDE_FAR", {0.0f, routeY, -180.0f});
    AddNode("WING_SPINE_3", {0.0f, routeY, -174.0f});
    AddNode("APRON_SIDE_MIDDLE", {0.0f, routeY, -140.0f});
    AddNode("WING_SPINE_2", {0.0f, routeY, -134.0f});
    AddNode("APRON_SIDE_NEAR", {0.0f, routeY, -100.0f});
    AddNode("WING_SPINE_1", {0.0f, routeY, -94.0f});
    AddNode("APRON_BACK_ROW", {0.0f, routeY, -60.0f});
    Connect("A_CENTER", "APRON_ENTRY", 15.0f);
    Connect("APRON_ENTRY", "APRON_GATEWAY", 14.0f, false);
    Connect("APRON_GATEWAY", "APRON_SIDE_FAR", 12.0f, false);
    Connect("APRON_SIDE_FAR", "WING_SPINE_3", 11.0f, false);
    Connect("WING_SPINE_3", "APRON_SIDE_MIDDLE", 11.0f, false);
    Connect("APRON_SIDE_MIDDLE", "WING_SPINE_2", 11.0f, false);
    Connect("WING_SPINE_2", "APRON_SIDE_NEAR", 11.0f, false);
    Connect("APRON_SIDE_NEAR", "WING_SPINE_1", 10.0f, false);
    Connect("WING_SPINE_1", "APRON_BACK_ROW", 10.0f, false);

    // A separate gate-side apron lane gives all four facade stands an approach.
    constexpr float gateXs[] = {-33.75f, -11.25f, 11.25f, 33.75f};
    AddNode("GATE_ROW_W", {-75.0f, routeY, -60.0f});
    AddNode("GATE_ROW_1", {gateXs[0], routeY, -60.0f});
    AddNode("GATE_ROW_2", {gateXs[1], routeY, -60.0f});
    AddNode("GATE_ROW_3", {gateXs[2], routeY, -60.0f});
    AddNode("GATE_ROW_4", {gateXs[3], routeY, -60.0f});
    AddNode("GATE_ROW_E", {75.0f, routeY, -60.0f});
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
        AddNode(westId, {-15.0f, routeY, z});
        AddNode(eastId, {15.0f, routeY, z});
        const std::string spineId = row == 0 ? "WING_SPINE_1" :
            row == 1 ? "WING_SPINE_2" : "WING_SPINE_3";
        Connect(spineId, westId, 9.0f, false);
        Connect(spineId, eastId, 9.0f, false);
        Connect(westId, eastId, 9.0f, false);
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

std::vector<std::string> AirportGround::FindRouteNodeIds(
    const std::string& startNodeId, const std::string& destinationNodeId) const
{
    const TaxiwayNode* start = FindNode(startNodeId);
    const TaxiwayNode* destination = FindNode(destinationNodeId);
    if (start == nullptr || destination == nullptr ||
        start->blockedByObstacle || destination->blockedByObstacle)
        return {};
    if (startNodeId == destinationNodeId) return {startNodeId};

    std::queue<std::string> frontier;
    std::unordered_map<std::string, std::string> predecessor;
    frontier.push(startNodeId);
    predecessor.emplace(startNodeId, "");

    while (!frontier.empty())
    {
        const std::string currentId = frontier.front();
        frontier.pop();
        const TaxiwayNode* current = FindNode(currentId);
        if (current == nullptr) continue;
        for (const std::string& neighborId : current->connectedNodeIds)
        {
            const TaxiwayNode* neighbor = FindNode(neighborId);
            if (neighbor == nullptr || neighbor->blockedByObstacle) continue;
            const bool sectionOpen = std::any_of(edges.begin(), edges.end(),
                [&currentId, &neighborId](const TaxiwayEdge& edge)
                {
                    return edge.open &&
                        ((edge.fromNodeId == currentId && edge.toNodeId == neighborId) ||
                         (edge.fromNodeId == neighborId && edge.toNodeId == currentId));
                });
            if (!sectionOpen) continue;
            if (predecessor.find(neighborId) != predecessor.end()) continue;
            predecessor.emplace(neighborId, currentId);
            if (neighborId == destinationNodeId)
            {
                std::vector<std::string> route;
                for (std::string id = destinationNodeId; !id.empty(); id = predecessor.at(id))
                    route.push_back(id);
                std::reverse(route.begin(), route.end());
                return route;
            }
            frontier.push(neighborId);
        }
    }
    return {};
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

        bool needsJoinPad = pavementNeighbors.size() >= 3;
        if (pavementNeighbors.size() == 2 && pavementNeighbors[0] && pavementNeighbors[1])
        {
            const float ax = pavementNeighbors[0]->position.x - node.position.x;
            const float az = pavementNeighbors[0]->position.z - node.position.z;
            const float bx = pavementNeighbors[1]->position.x - node.position.x;
            const float bz = pavementNeighbors[1]->position.z - node.position.z;
            const float aLength = std::sqrt(ax * ax + az * az);
            const float bLength = std::sqrt(bx * bx + bz * bz);
            if (aLength > 0.01f && bLength > 0.01f)
                // Neighbour vectors point away from the node. Opposite vectors
                // mean a straight lane; only actual bends need a join pad.
                needsJoinPad = (ax * bx + az * bz) / (aLength * bLength) > -0.85f;
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

    // Operational closures remain visible as a red transverse bar. Route
    // searches skip the same edge, so the visual closure and graph agree.
    for (const TaxiwayEdge& edge : edges)
    {
        if (edge.open) continue;
        const TaxiwayNode* from = FindNode(edge.fromNodeId);
        const TaxiwayNode* to = FindNode(edge.toNodeId);
        if (from == nullptr || to == nullptr) continue;
        const float dx = to->position.x - from->position.x;
        const float dz = to->position.z - from->position.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        if (length <= 0.01f) continue;
        const float ux = dx / length;
        const float uz = dz / length;
        const float px = -uz * edge.width * 0.5f;
        const float pz = ux * edge.width * 0.5f;
        const float barHalfLength = 1.25f;
        const Vector3 center{(from->position.x + to->position.x) * 0.5f,
            0.19f, (from->position.z + to->position.z) * 0.5f};
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
    constexpr float runwayExitXs[] = {-85.0f, 0.0f, 85.0f};
    for (int exitSide : {0, 2})
    {
        const float x = runwayExitXs[exitSide];
        drawRunwayHold(x, runways[0].center.z + runways[0].width * 0.5f, 1.0f);
        drawRunwayHold(x, runways[0].center.z - runways[0].width * 0.5f, -1.0f);
        drawRunwayHold(x, runways[1].center.z + runways[1].width * 0.5f, 1.0f);
        drawRunwayHold(x, runways[1].center.z - runways[1].width * 0.5f, -1.0f);
        drawRunwayHold(x, runways[2].center.z + runways[2].width * 0.5f, 1.0f);
    }
}
