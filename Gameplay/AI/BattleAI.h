#pragma once

#include "Gameplay/Doctrine/Doctrine.h"
#include "Gameplay/Result.h"
#include "Gameplay/Sim/Sim.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Potato::Gameplay {

class BattleController;
class BattleMap;
struct SquadTemplate;

// Symmetric AI opponent (GDD FR11): the enemy runs the SAME doctrine
// interpreter, CP economy, and command API as the player — victories
// are earned, not stat-inflated. Personality priors bias CHOICES
// (deployment order, sheet composition, arrow shape, feints,
// intervention policy) — never stats, never hidden information.
//
// BattleAI is a pure CLIENT of BattleController's public API: it holds
// no friend access and mutates battle state only through
// DeploySquad/SetSheet/DrawArrow/Issue*/SetCloudIntel — the identical
// surface a player uses. The host calls Plan() during Planning and
// Act() between Tick()s during Execution.
//
// Truth boundary: Act reads its own side's squad fields and its own
// Fog(side) view only — never enemy Squads() truth (same discipline
// as future presentation code; mechanical guard deferred).
//
// Determinism: a private Prng seeded from (seed, side) — outside the
// sim stream; AI decisions enter the record as ordinary SimEvents.
enum class Prior : std::uint8_t { Aggressive, Defensive, Cunning };

class BattleAI {
public:
    BattleAI(std::uint64_t seed, int side, Prior prior,
             const BattleMap& map, const DoctrineLibrary& cards);

    Prior GetPrior() const { return prior_; }
    int Side() const { return side_; }

    // Card preference table — the personality spec, exposed so tests
    // pin priors directly. Higher = preferred for the sheet.
    static int ScoreCard(const DoctrineCard& card, Prior prior);

    // --- Planning (call while bc.Beat() == Planning) ---
    // Deploys `troops` onto `startRegions` in prior-chosen order,
    // builds a prior-scored sheet per squad from `cardPool`
    // (>=3 cards or squads keep empty sheets), draws prior-shaped
    // arrows toward `objective`, and Cunning plants feints. Returns
    // false (no mutation) on unusable input.
    bool Plan(BattleController& bc,
              const std::vector<SquadTemplate>& troops,
              const std::vector<std::size_t>& startRegions,
              const std::vector<std::string>& cardPool,
              std::size_t objective);

    // Planning seam for hosts that deploy via marshaled rows
    // (Campaign's DeployAssembly) rather than Plan(): everything
    // Plan() does AFTER deployment — slot memory read from the
    // already-set sheets, prior-shaped arrows toward `objective`,
    // and Cunning's feints. `objective` is HOST-SUPPLIED (e.g.
    // the player warband's region as the encounter authored it —
    // the AI never derives one from enemy squad truth). Same
    // beat gate; false (no mutation) when no own squad is fielded.
    bool PlanDeployed(BattleController& bc, std::size_t objective);

    // --- Execution (call once between Tick()s) ---
    // Issues at most one intervention per squad per call, in
    // canonical squad-index order, through the Issue* gate — same
    // CheckCommand, same CP pool, same apply-at-next-tick latency.
    void Act(BattleController& bc);

    // BFS shortest hop path (canonical neighbor order); empty when
    // unreachable; {from} when from == to.
    static std::vector<std::size_t> ShortestPath(const BattleMap& map,
                                                 std::size_t from,
                                                 std::size_t to);

private:
    // One intervention attempt per squad, priority-ordered per prior.
    void ActOnSquad(BattleController& bc, std::size_t index);
    // The Cunning plant — one confident false signature on a
    // neighbor the arrow doesn't touch. Shared by Plan() (fresh
    // deploy) and PlanDeployed() (marshaled rows).
    void PlantFeint(BattleController& bc, int squadIndex,
                    std::size_t region,
                    const std::vector<std::size_t>& path);

    int side_ = 0;
    Prior prior_ = Prior::Aggressive;
    const BattleMap& map_;
    const DoctrineLibrary& cards_;
    // Own stream — never the sim's. Reserved for stochastic choices
    // (v0 policies are fully deterministic; the member documents the
    // contract that AI draws may NEVER come from sim's Prng).
    Prng rng_;
    std::size_t objective_ = 0;
    int overrideSlot_ = -1; // prior-preferred sheet slot (aggressive)
    int braceSlot_ = -1;    // brace slot (defensive)
    std::vector<char> probed_; // Cunning: don't re-probe regions
    int entangleTick_ = -1;    // last tick an entangle was issued —
                               // pending pairs stay un-entangled until
                               // apply, so a double-Act gap would
                               // re-issue and double-spend CP
};

} // namespace Potato::Gameplay
