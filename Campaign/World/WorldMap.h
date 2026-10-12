#pragma once

#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Potato::Campaign {

// Campaign-scope world geography (Story 12.1, D-ARCH-10): the open
// strategic map — a region/POI graph with march routes, initial
// faction control, and dual-layer marks. Static content: read at
// campaign boundaries, never inside the battle tick. Runtime state
// (warband position, control drift) is WorldState's job (12.2+).
//
// Flag bits reuse the Gameplay/Map vocabularies (TERRAIN_*,
// STRATEGIC_*, HIST_*, MYTH_*) — one flag language for both layers.
//
// Wire shape (one file per world):
//   {"schema":"potato.world/1","id":"republic_fall","name":"…",
//    "nodes":[{"id":"kaifeng","name":"開封","terrain":["road"],
//              "control":"player","map":"ch01_plain",
//              "center":[12,8]}, …],
//    "routes":[{"a":"kaifeng","b":"luoyang","days":3}, …],
//    "start":"kaifeng"}

// Initial faction control of a node. Runtime control drift belongs
// to WorldState — this is the campaign-start disposition only.
enum class WorldControl : std::uint8_t { Neutral, Player, Rival };

struct WorldNode {
    std::string id;       // canonical campaign-scope region id
    std::string name;     // display name, may carry UTF-8 CJK
    std::uint32_t terrain = 0;     // Gameplay TERRAIN_* bits
    std::uint32_t strategic = 0;   // STRATEGIC_* bits
    std::uint32_t historical = 0;  // HIST_* bits
    std::uint32_t myth = 0;        // MYTH_* bits
    WorldControl control = WorldControl::Neutral;
    // Optional potato.map/1 content ref — the battle map an
    // encounter on this node marshals into (Story 12.5). Shape-
    // validated only; existence is the encounter pipeline's check.
    std::string map;
    // Optional presentation hint; no world semantics.
    std::int64_t centerX = 0;
    std::int64_t centerY = 0;
    bool hasCenter = false;
};

class WorldMap {
public:
    static constexpr std::size_t NO_NODE = ~std::size_t{0};

    // potato.world/1 file → gated load → validation.
    static Gameplay::Result<WorldMap> Load(std::string_view path);
    // Validate a DOM root (schema must already be gated for file
    // loads). Pure: failure produces no partial map.
    static Gameplay::Result<WorldMap> FromJson(
        const Gameplay::JsonValue& root);

    std::size_t NodeCount() const { return nodes_.size(); }
    // Precondition: index < NodeCount() — callers must check
    // NodeIndexOf misses (NO_NODE) before indexing; OOB is UB.
    const WorldNode& NodeAt(std::size_t index) const {
        assert(index < nodes_.size());
        return nodes_[index];
    }
    // nullptr on miss.
    const WorldNode* FindNode(std::string_view id) const;
    // Node index (file order) — canonical key for WorldState and
    // any per-node array downstream. Linear scan — intended scale
    // is tens of nodes per world.
    std::size_t NodeIndexOf(std::string_view id) const;
    // Undirected neighbors with march cost, in route-declaration
    // order (deterministic). Precondition: index < NodeCount().
    const std::vector<std::pair<std::size_t, std::int64_t>>&
    Neighbors(std::size_t index) const {
        assert(index < adjacency_.size());
        return adjacency_[index];
    }

    const std::string& Id() const { return id_; }
    const std::string& Name() const { return name_; }
    // The warband's spawn node (Story 12.3's anchor).
    std::size_t StartIndex() const { return start_; }

private:
    std::string id_;
    std::string name_;
    std::vector<WorldNode> nodes_;
    std::vector<std::vector<std::pair<std::size_t, std::int64_t>>>
        adjacency_;
    std::size_t start_ = 0;
};

struct RejectedWorld {
    std::filesystem::path path;
    std::string error;
    std::string reason;
};

struct WorldLoadResult {
    bool ok = false;
    std::string error;
    std::string reason;
    std::vector<RejectedWorld> rejected; // per-file failures
};

// Boot-time registry (ChapterLibrary precedent): worlds are
// content, not code. Built once by Load(), immutable thereafter
// (NFR9). Per-file isolation: a bad world file is rejected and
// logged, never fails the library; Load fails wholesale only if the
// directory itself is unreadable or the file count overflows.
class WorldLibrary {
public:
    static constexpr std::string_view SCHEMA = "potato.world/1";
    static constexpr std::size_t MAX_WORLDS = 64;
    static constexpr std::size_t MAX_NODES = 256;
    static constexpr std::size_t MAX_ID_LEN = 64;
    static constexpr std::size_t MAX_NAME_LEN = 128;
    static constexpr std::size_t MAX_REF_LEN = 256;
    static constexpr std::int64_t MAX_ROUTE_DAYS = 30;

    // Sorted-filename iteration, per-file rejection into
    // result.rejected. Directory unreadable -> !ok.
    static WorldLoadResult Load(const std::filesystem::path& dir,
                                WorldLibrary& out);

    const WorldMap* Find(std::string_view id) const;
    std::size_t Size() const { return worlds_.size(); }
    // Canonical order: world id.
    const std::vector<WorldMap>& Worlds() const { return worlds_; }

private:
    std::vector<WorldMap> worlds_;
};

} // namespace Potato::Campaign
