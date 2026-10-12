#pragma once

#include "Gameplay/Result.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace Potato::Campaign {
class WorldMap;
}

namespace Potato::Game {

// WorldProjection — campaign world graph → pixel layout (Story
// 12.9). The WorldMap analog of MapProjection: WorldMap is a
// different type (node graph + route day costs, not a battle
// region map), so this lives in its own file rather than
// overloading 8.1's class.
//
// Two layout modes, both integer-only and PRNG-free:
//   hinted — EVERY node carries a centerX/Y hint: content coords
//     are linearly normalized into the view bounds
//     (aspect-preserved, centered).
//   grid   — any node lacks a hint: index-ordered grid,
//     cell = view / ceil(sqrt(n)) — file order is canonical, so
//     the layout is stable across loads.
//
// Edges come out deduplicated (each undirected route once, in
// node-index order).

struct ProjectedNode {
    std::size_t index = 0;
    int x = 0, y = 0;           // pixel center
};

class WorldProjection {
public:
    static Gameplay::Result<WorldProjection> Build(
        const Campaign::WorldMap& map, int w, int h, int margin);

    const std::vector<ProjectedNode>& Nodes() const {
        return nodes_;
    }
    const ProjectedNode& At(std::size_t index) const {
        return nodes_[index];
    }
    // (lower, higher) index pairs — each route once.
    const std::vector<std::pair<std::size_t, std::size_t>>&
    Edges() const {
        return edges_;
    }
    bool Hinted() const { return hinted_; }

private:
    std::vector<ProjectedNode> nodes_;
    std::vector<std::pair<std::size_t, std::size_t>> edges_;
    bool hinted_ = false;
};

} // namespace Potato::Game
