#pragma once

#include "airport_terminal.h"

#include <string>
#include <vector>

struct TaxiwayNode
{
    std::string id;
    Vector3 position;
    std::vector<std::string> connectedNodeIds;
};

struct TaxiwayEdge
{
    std::string fromNodeId;
    std::string toNodeId;
    float width;
    bool drawSurface;
};

// Runway and taxiway geometry share their coordinates with the terminal's stands.
class AirportGround
{
public:
    explicit AirportGround(const AirportTerminal& terminal);

    void Draw() const;
    const std::vector<TaxiwayNode>& GetTaxiwayNodes() const { return nodes; }
    const std::vector<TaxiwayEdge>& GetTaxiwayEdges() const { return edges; }
    const TaxiwayNode* FindNode(const std::string& id) const;

private:
    void AddNode(const std::string& id, Vector3 position);
    void Connect(const std::string& from, const std::string& to, float width, bool drawSurface = true);

    std::vector<TaxiwayNode> nodes;
    std::vector<TaxiwayEdge> edges;
};
