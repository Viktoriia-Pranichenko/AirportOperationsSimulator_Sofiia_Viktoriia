#pragma once

#include "airport_terminal.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

struct TaxiwayNode
{
    std::string id;
    Vector3 position;
    std::vector<std::string> connectedNodeIds;
    std::vector<std::size_t> connectedEdgeIndices;
    bool blockedByObstacle{false};
};

struct TaxiwayEdge
{
    std::string fromNodeId;
    std::string toNodeId;
    float width;
    bool drawSurface;
    bool stopAtHoldLine;
    float holdLineZ;
    bool open{true};
    Vector3 fromPosition;
    Vector3 toPosition;
};

struct AirportRunway
{
    std::string id;
    Vector3 center;
    float length;
    float width;
    std::string westEndNumber;
    std::string eastEndNumber;
    bool available{true};
};

enum class TaxiRoutePurpose
{
    General,
    Arrival,
    Departure,
    SharedTaxiway
};

// Runway and taxiway geometry share their coordinates with the terminal's stands.
class AirportGround
{
public:
    explicit AirportGround(const AirportTerminal& terminal);

    void Draw(Color grassColor) const;
    const std::vector<AirportRunway>& GetRunways() const { return runways; }
    const std::vector<TaxiwayNode>& GetTaxiwayNodes() const { return nodes; }
    const std::vector<TaxiwayEdge>& GetTaxiwayEdges() const { return edges; }
    const TaxiwayNode* FindNode(const std::string& id) const;
    // Closed sections and blocked nodes are omitted from future route searches.
    bool SetTaxiwaySectionOpen(const std::string& fromNodeId,
        const std::string& toNodeId, bool open);
    bool SetNodeBlocked(const std::string& nodeId, bool blocked);
    bool SetRunwayAvailable(const std::string& runwayId, bool available);
    bool IsRunwayOrApproachNode(const std::string& nodeId) const;
    std::vector<std::string> FindRouteNodeIds(
        const std::string& startNodeId, const std::string& destinationNodeId,
        TaxiRoutePurpose purpose = TaxiRoutePurpose::General) const;

private:
    struct TaxiwayJunction
    {
        Vector3 position;
        float widestLane;
    };

    void AddNode(const std::string& id, Vector3 position);
    void Connect(const std::string& from, const std::string& to, float width,
        bool drawSurface = true, bool stopAtHoldLine = false, float holdLineZ = 0.0f);

    std::vector<TaxiwayNode> nodes;
    std::vector<TaxiwayEdge> edges;
    std::vector<AirportRunway> runways;
    std::unordered_map<std::string, std::size_t> nodeIndexById;
    std::vector<TaxiwayJunction> taxiwayJunctions;
};
