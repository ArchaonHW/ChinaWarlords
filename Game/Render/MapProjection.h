#pragma once

#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <unordered_map>
#include <vector>

namespace Potato::Gameplay { class BattleMap; }

namespace Potato::Game {

// MapProjection — region graph → pixel layout (Story 8.1).
// BattleMap is a region graph, not a tile grid; this is the
// projection the sim deferred to Epic F.
//
// Two layout modes, both integer-only and PRNG-free:
//   hinted — EVERY region carries a centerX/Y hint: content
//     coords are linearly normalized into the view bounds
//     (aspect-preserved, centered).
//   grid   — any region lacks a hint: index-ordered grid,
//     cell = view / ceil(sqrt(n)) — file order is canonical,
//     so the layout is stable across loads.
//
// Edges come out deduplicated (each undirected adjacency once,
// in region-index order).

struct ProjectedRegion {
    std::size_t index = 0;
    int x = 0, y = 0;           // pixel center
};

class MapProjection {
public:
    static Gameplay::Result<MapProjection> Build(
        const Gameplay::BattleMap& map, int w, int h, int margin);

    const std::vector<ProjectedRegion>& Regions() const {
        return regions_;
    }
    const ProjectedRegion& At(std::size_t index) const {
        return regions_[index];
    }
    // (lower, higher) index pairs — each edge once.
    const std::vector<std::pair<std::size_t, std::size_t>>&
    Edges() const {
        return edges_;
    }
    bool Hinted() const { return hinted_; }
    // Inverse lookup (Epic 13 input): pixel -> nearest region
    // whose projected center lies within `radius` px; -1 when
    // nothing is near. Distance is squared-integer; ties break
    // to the lowest index — canonical order, like everything
    // else.
    int Hit(int x, int y, int radius) const;

private:
    std::vector<ProjectedRegion> regions_;
    std::vector<std::pair<std::size_t, std::size_t>> edges_;
    // Uniform-grid broadphase for pixel hit tests, inspired by
    // StonePotatoes/src/ai/SpatialGrid.h. This presentation-only index uses
    // integer screen coordinates and stores region indices in canonical order.
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> hitCells_;
    bool hinted_ = false;
};

} // namespace Potato::Game
