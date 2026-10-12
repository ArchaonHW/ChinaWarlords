#pragma once

#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Gameplay {

// Region/node-graph battlefield model (architecture: Gameplay/Map).
// NOT a tile grid — presentation concerns live in Epic F.
// Immutable after load; integer-only (determinism rule).

enum Terrain : std::uint32_t {
    TERRAIN_NONE       = 0,
    TERRAIN_WATER      = 1u << 0,
    TERRAIN_RIVER      = 1u << 1,
    TERRAIN_ROAD       = 1u << 2,
    TERRAIN_FOREST     = 1u << 3,
    TERRAIN_HIGHLAND   = 1u << 4,
    TERRAIN_CHOKEPOINT = 1u << 5,
    TERRAIN_OPEN       = 1u << 6,
};

enum Strategic : std::uint32_t {
    STRATEGIC_NONE    = 0,
    STRATEGIC_FERRY   = 1u << 0,
    STRATEGIC_DEPOT   = 1u << 1,
    STRATEGIC_VILLAGE = 1u << 2,
};

enum HistMark : std::uint32_t {
    HIST_NONE        = 0,
    HIST_GRAIN_ROUTE = 1u << 0,
    HIST_TELEGRAPH   = 1u << 1,
    HIST_SUPPLY_LINE = 1u << 2,
};

enum MythMark : std::uint32_t {
    MYTH_NONE        = 0,
    MYTH_SHRINE      = 1u << 0,
    MYTH_SPIRIT_ROAD = 1u << 1,
    MYTH_HAUNTED     = 1u << 2,
};

struct Region {
    std::string id;    // content reference (unique within the file)
    std::string name;  // display name, may carry UTF-8 CJK
    std::uint32_t terrain = 0;
    std::uint32_t strategic = 0;
    std::uint32_t historical = 0;
    std::uint32_t myth = 0;
    // Optional content hint for Plan/presentation; no sim semantics yet.
    std::int64_t centerX = 0;
    std::int64_t centerY = 0;
    bool hasCenter = false;
};

// GDD counts shrines among strategic points (capturable myth assets).
inline bool IsStrategicPoint(const Region& r) {
    return (r.strategic & (STRATEGIC_FERRY | STRATEGIC_DEPOT | STRATEGIC_VILLAGE)) != 0 ||
           (r.myth & MYTH_SHRINE) != 0;
}

class BattleMap {
public:
    static constexpr std::size_t NO_REGION = ~std::size_t{0};
    // Hard cap on regions per battle map. Per-region sim state
    // (fog certainty, village tracks, MythField levels, checksum
    // folds) scales with this, and Campaign::MythState persists at
    // most this many regions per chapter — keep the two in lockstep.
    static constexpr std::size_t MAX_REGIONS = 1024;

    // potato.map/1 file → gated load → validation.
    static Result<BattleMap> Load(std::string_view path);
    // Validate a DOM root (schema must already be gated for file loads).
    // Pure: failure produces no partial map.
    static Result<BattleMap> FromJson(const JsonValue& root);

    std::size_t RegionCount() const { return regions_.size(); }
    // Precondition: index < RegionCount() — callers must check
    // RegionIndexOf misses (NO_REGION) before indexing; OOB is UB.
    const Region& RegionAt(std::size_t index) const {
        assert(index < regions_.size());
        return regions_[index];
    }
    // nullptr on miss.
    const Region* FindRegion(std::string_view id) const;
    // Region index (file order) — canonical key for CertaintyField,
    // BattlePlan coverage, and any per-region array downstream.
    // Linear scan — intended scale is tens of regions per battle map.
    std::size_t RegionIndexOf(std::string_view id) const;
    // Undirected neighbors, in edge-declaration order (deterministic).
    // Precondition: index < RegionCount().
    const std::vector<std::size_t>& Neighbors(std::size_t index) const {
        assert(index < adjacency_.size());
        return adjacency_[index];
    }

    const std::string& Id() const { return id_; }
    const std::string& Name() const { return name_; }

private:
    std::string id_;
    std::string name_;
    std::vector<Region> regions_;
    std::vector<std::vector<std::size_t>> adjacency_;
};

} // namespace Potato::Gameplay
