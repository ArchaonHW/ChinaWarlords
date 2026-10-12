#pragma once

#include "Gameplay/Sim/Sim.h" // TICK_RATE_HZ

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Potato::Gameplay {

class BattleMap;
struct SimEvent;
struct Squad;

// GovernanceField — battle-scope tracking of field conduct (Epic 4,
// Story 4.1). Watches true squad positions each tick (governance
// events are OUTCOMES, not perceptions — the truth boundary only
// governs belief reads) and emits typed SimEvents destined for the
// campaign ledger's five-account fold (Story 4.3).
//
// Tracked deeds:
//   * Village occupation — exclusive presence held for OCCUPY_TICKS.
//   * Village burning — an authored doctrine `burn` action applying
//     on an unburned village the squad still holds.
//   * Convoy escort — a convoy completing its route (ConvoyArrived).
//   * Convoy raid — a convoy caught in a region with enemy presence
//     and no defender.
//
// Everything is deterministic: fixed scan order (map order for
// villages, convoy index for convoys, squad index for witnesses),
// no PRNG draws.

// Consecutive exclusive-presence ticks a village needs before its
// occupier latches (4 s at 20 Hz).
constexpr int OCCUPY_TICKS = 4 * TICK_RATE_HZ;
// Ticks a convoy spends on each leg — the Slow squad edge cost
// (TICKS_PER_EDGE_BASE at SpeedMilli(Slow) = 600 -> 66).
constexpr int CONVOY_LEG_TICKS = 66;
// Convoy indices ride SimEvent::aux (wire gate 0..6, bounded by the
// InterventionKind domain) — the cap keeps them inside it.
constexpr std::size_t MAX_CONVOYS = 4;

// Per-village tracker. `claimant` is this tick's exclusive-presence
// side (-1 = none or contested); `dwell` counts consecutive ticks of
// the same claimant. `occupier` is the latched holder (-1 = unheld)
// — cleared when exclusive control is lost, so a recapture re-dwells
// and re-emits. `burned` is permanent ash: no occupation, no re-burn.
struct VillageTrack {
    std::size_t region = 0;
    int claimant = -1;
    int dwell = 0;
    int occupier = -1;
    bool burned = false;
};

// A noncombatant convoy marching a fixed route (village-supply
// setpiece). Stands at path[cursor]; after CONVOY_LEG_TICKS it
// advances one node (departure-region semantics — the same
// convention squads use mid-edge). `active` goes false on raid or
// arrival; the convoy then leaves the field.
struct Convoy {
    int side = 0;                      // owner
    std::vector<std::size_t> path;     // >= 2 regions
    std::size_t cursor = 0;            // standing at path[cursor]
    int legProgress = 0;               // ticks on the current leg
    bool active = true;
};

class GovernanceField {
public:
    // Indexes the map's village regions. Borrowed ref — the map must
    // outlive the field (same contract as BattleController).
    void Init(const BattleMap& map);

    // Planning-only convoy spawn (the Planning beat gate lives on
    // the controller wrapper — Field() is const, so only the
    // controller's own Tick path can mutate this field). Path rules
    // match plan arrows: in-bounds, pairwise-adjacent, no revisits,
    // length 2..RegionCount. The convoy stands at path[0] until
    // Execution ticks march it — a raider camped at the spawn node
    // catches it on tick 0 (spawn placement is a plan-time risk,
    // truthfully punished).
    bool SpawnConvoy(int side, std::vector<std::size_t> path);

    // Delta-time burn attempt (the controller routes Burn pending
    // deltas here). Succeeds only if the squad is effective, still
    // Holding, and standing on an unburned village region — a squad
    // that walked off earlier in the same delta list can't burn.
    // On success the village is flagged burned and a VillageBurned
    // event is returned for the battle log.
    // Design note: arson is instant, unlike occupation's dwell —
    // deliberate asymmetry. Burning is a DEED (priced by the ledger
    // fold via 民心 debit + 軍威 ratchet), not a hold; a contested
    // burn is still a burn — the perpetrator is recorded and the
    // accounting layers price it. A squad driving a plan arrow burns
    // Holding, then marches away the same tick — scorched-earth
    // columns are authored behavior, authoredly expensive.
    std::optional<SimEvent> TryBurn(std::size_t squadIndex,
                                    const Squad& sq, int tick);

    // End-of-tick scan, after squad movement: village dwell/latch,
    // convoy march -> raid -> arrival. Events come back in canonical
    // emission order (villages in map order, then convoys by index).
    // Note: deeds on the battle's closing tick are still emitted —
    // they precede BeatChanged/ResultDeclared in the log; and a
    // convoy mid-route when the battle closes simply never reports
    // (silence = unsettled — the ledger fold treats an in-flight
    // convoy as no outcome).
    std::vector<SimEvent> Tick(const std::vector<Squad>& squads,
                               int tick);

    const std::vector<VillageTrack>& Villages() const {
        return villages_;
    }
    const std::vector<Convoy>& Convoys() const { return convoys_; }

private:
    // First effective non-Routing squad of `side` standing at
    // `region`, or -1. Witness/escort/raider fields need one
    // deterministic squad — canonical order is squad index.
    static int FirstAt(const std::vector<Squad>& squads, int side,
                       std::size_t region);

    const BattleMap* map_ = nullptr;
    std::vector<VillageTrack> villages_; // map order
    std::vector<Convoy> convoys_;        // spawn order
};

} // namespace Potato::Gameplay
