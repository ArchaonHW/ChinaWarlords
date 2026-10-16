#pragma once

#include "Campaign/World/WorldMap.h"
#include "Campaign/Quantum/QuantumOracle.h" // 13.6 decision seam
#include "Gameplay/Result.h"
#include "Gameplay/Sim/SimulationLod.h"
#include "Gameplay/Sim/Sim.h" // Prng — the dedicated world stream

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Gameplay {
class JsonValue;
}

namespace Potato::Campaign {

// World runtime state (Story 12.2, D-ARCH-10): the living side of
// the open campaign map — day counter, warband anchor, per-node
// control, resolved-POI set, and a pending event queue resolved at
// day-scale beats.
//
// NOT a second tick sim: resolution is event-granular, ordered,
// integer-only, and draws from a dedicated SplitMix64 stream that
// is NOT the battle's PRNG. Zero file I/O, no exceptions.
//
// Persisted as versioned doc `potato.worldstate/1` (MythState
// potato.myth/1 precedent), normally embedded in the campaign
// save envelope. It can also be serialized on its own.
//
// Wire shape:
//   {"schema":"potato.worldstate/1","world":"republic_fall",
//    "day":4,"warband":"kaifeng",
//    "control":{"kaifeng":"player","luoyang":"rival"},
//    "resolved":["longmen_shrine"],"rng":12345,
//    "agents":[{"id":7,"public_node":"kaifeng","lod":{...}}],
//    "queue":[{"day":6,"seq":0,"kind":"control",
//              "node":"luoyang","control":"neutral"}]}
//
// Canonical form: only non-Neutral control is stored (silence is
// ground state); `resolved` and `queue` are sorted; `rng` is the
// raw SplitMix64 counter so a reloaded campaign draws the same
// sequence.

enum class WorldEventKind : std::uint8_t {
    SetControl, // node changes faction control
    Resolve,    // mark a POI/encounter id resolved
    March,      // warband arrives at node (12.3)
    Sight,      // hearsay marker: rival banners seen (12.3)
};

struct WorldEvent {
    std::int64_t day = 0;         // beat at which it applies
    std::uint64_t seq = 0;        // emission tie-break
    WorldEventKind kind = WorldEventKind::SetControl;
    std::string node;             // world node id (the target)
    WorldControl control = WorldControl::Neutral; // SetControl arg
};

// Hearsay marker — rival warbands exist only as sightings the
// camp heard about (node, freshest day, count). No live position,
// no truth the player can read: uncertainty is the data.
struct SightingMark {
    std::int64_t day = 0;
    std::int64_t count = 0;
};

// Campaign-visible agent projection used by strategic simulation LOD.
// `publicNode` is the location known to the campaign layer, not a source
// of hidden enemy position. Importance factors are likewise caller-owned
// observations or public strategic data; this record stores no battle truth.
struct WorldAgentState {
    std::uint64_t id = 0; // stable within this world's campaign save
    std::string publicNode;
    Gameplay::SimulationLodFactors lodFactors;

    friend bool operator==(const WorldAgentState&,
                           const WorldAgentState&) = default;
};

class WorldState {
public:
    static constexpr std::string_view SCHEMA = "potato.worldstate/1";
    static constexpr std::size_t MAX_EVENTS = 1024;   // queued
    static constexpr std::size_t MAX_RESOLVED = 4096;
    static constexpr std::size_t MAX_AGENTS = 100000;
    static constexpr std::size_t MAX_ID_LEN = 64;
    static constexpr std::int64_t MAX_DAY = 36500;    // ~100 yrs

    // Fresh campaign state bound to a world doc: day 0, warband at
    // map.StartIndex(), control seeded from node `control` fields,
    // world PRNG at `seed`.
    static Gameplay::Result<WorldState> Init(const WorldMap& map,
                                             std::uint64_t seed);

    std::int64_t Day() const { return day_; }
    const std::string& WorldId() const { return worldId_; }
    // Warband anchor node id; empty only in a default state.
    const std::string& WarbandAt() const { return warband_; }
    // Neutral on miss (silence = unremarkable ground state).
    WorldControl ControlAt(std::string_view node) const;
    bool IsResolved(std::string_view node) const {
        return resolved_.count(std::string(node)) != 0;
    }
    // Mark a POI/encounter id resolved directly (12.5): the
    // resolved set accepts arbitrary ids, not just map nodes —
    // queue Resolve events stay node-keyed, but encounters live
    // in their own id space and settle outside the beat queue.
    // Idempotent; false only on bound/empty-id rejection.
    bool MarkResolved(std::string_view id);
    // Resolved-set occupancy — the settle ceremony (12.5)
    // preflights capacity before committing so MarkResolved
    // cannot fail post-mutation.
    std::size_t ResolvedCount() const { return resolved_.size(); }
    // Canonical order: (day, seq, insertion-stable).
    const std::vector<WorldEvent>& Pending() const { return queue_; }

    // Queue an event: node must exist in `map`, day >= Day(), queue
    // under MAX_EVENTS. Mutation only on success.
    Gameplay::Result<bool> Enqueue(const WorldMap& map,
                                   WorldEvent ev);

    // Advance `days` beats (>= 0), draining every queued event with
    // day <= new day in canonical (day, seq, insertion) order.
    // Integer math; no I/O; no exceptions. An event whose node has
    // since vanished from the map is skipped — never applied — but
    // a skipped March still releases the in-flight marker (the
    // warband stays put; it isn't walking to a deleted node).
    Gameplay::Result<int> ResolveBeats(const WorldMap& map,
                                       std::int64_t days);

    // The world's dedicated PRNG stream. Draw order IS the
    // canonical order — draw only from inside beat resolution and
    // other canonical paths, never from presentation.
    std::uint64_t Draw() { return rng_.Next(); }

    // --- 13.6: quantum decision seam (D-ARCH-11) ---
    //
    // Opt the world's probability questions into the quantum
    // oracle. The oracle owns a SECOND stream (seeded on enable),
    // so opting in never perturbs the classic Draw() sequence —
    // classical and oracle replays stay stream-compatible inside
    // their own lanes. Persisted under "oracle":{"seed","state"};
    // absent ⇒ classical. Enabling mid-campaign only affects draws
    // after the call — draw order stays canonical.
    Gameplay::Result<bool> EnableOracle(std::uint64_t seed,
                                        std::uint64_t state = 0);
    void DisableOracle() { oracle_.reset(); }
    bool HasOracle() const { return oracle_.has_value(); }

    // Bernoulli permille — true with probability permille/1000.
    // Oracle path routes through QuantumOracle::DrawPermille;
    // classical rolls Draw()%1000. permille<=0/​>=1000 answers
    // without consuming either stream — the degenerate contract
    // is identical across modes so draw counts stay aligned.
    bool DrawPermille(int permille);

    // Weighted categorical — index in [0,count); zero-weight
    // entries unreachable, -1 on empty/all-zero, single nonzero
    // weight answers without consuming either stream.
    int DrawCategorical(const int* weights, int count);
    int DrawCategorical(const std::vector<int>& weights) {
        return DrawCategorical(weights.data(),
                               static_cast<int>(weights.size()));
    }

    // Direct relocate seam (tests/dev tools; 12.3's march order
    // goes through the event queue instead — see March.cpp).
    // false if `node` isn't in `map`.
    bool SetWarband(std::string_view node, const WorldMap& map);

    // --- 12.3: movement & hearsay ---

    // State-owned emission counter for canonical event ordering.
    // Callers pass NextSeq() into WorldEvent::seq; persisted as
    // "seq" so replays emit identical queues.
    std::uint64_t NextSeq() { return nextSeq_++; }
    std::uint64_t SeqCounter() const { return nextSeq_; }

    // In-flight march: a pending March event marks the warband as
    // en route. Issued by March.cpp; cleared when the arrival
    // applies. Empty destination = not marching.
    bool Marching() const { return !marchingTo_.empty(); }
    const std::string& MarchDest() const { return marchingTo_; }
    std::int64_t MarchEta() const { return marchEta_; }
    // Begin/clear the in-flight marker — validation (adjacency,
    // cost) lives in IssueMarch; this just records intent.
    void BeginMarch(std::string_view dest, std::int64_t eta) {
        marchingTo_ = std::string(dest);
        marchEta_ = eta;
    }

    // Hearsay read view: node -> {freshest day, count}.
    const std::map<std::string, SightingMark>& Sightings() const {
        return sightings_;
    }

    // Persistent campaign agents are canonicalized by stable ID. The
    // replacement is all-or-nothing and accepts only public map nodes.
    Gameplay::Result<bool> SetAgents(
        const WorldMap& map, std::span<const WorldAgentState> agents);
    const std::vector<WorldAgentState>& Agents() const { return agents_; }
    // Pure projection into the Gameplay policy. Assignments are derived,
    // sorted by stable ID, and intentionally not persisted.
    Gameplay::Result<std::vector<Gameplay::SimulationLodAssignment>>
    PlanAgentLod(const Gameplay::SimulationLodPolicy& policy,
                 const Gameplay::SimulationBudget& budget) const;

    Gameplay::Result<Gameplay::JsonValue> ToJson() const;
    static Gameplay::Result<WorldState> FromJson(
        const Gameplay::JsonValue& doc);

private:
    std::string worldId_;
    std::int64_t day_ = 0;
    std::string warband_;
    std::map<std::string, WorldControl> control_; // non-Neutral only
    std::set<std::string> resolved_;
    std::vector<WorldEvent> queue_;
    Gameplay::Prng rng_{0};
    std::uint64_t nextSeq_ = 0;                   // emission counter
    std::string marchingTo_;                      // in-flight dest
    std::int64_t marchEta_ = 0;                   // arrival day
    std::map<std::string, SightingMark> sightings_;
    std::vector<WorldAgentState> agents_; // sorted by stable id
    std::optional<QuantumOracle> oracle_;   // 13.6 — second stream
    std::uint64_t oracleSeed_ = 0;          // persisted for resume
};

} // namespace Potato::Campaign
