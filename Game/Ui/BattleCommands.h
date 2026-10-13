#pragma once

#include "Gameplay/Command/Intervention.h"
#include "Gameplay/Fog/QuantumFog.h"
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Potato::Gameplay {
class BattleController;
class BattleMap;
} // namespace Potato::Gameplay

namespace Potato::Game {

// Battle commands (Story 13.6's input half): the Execution
// grammar — 點己方小隊 → 點動詞 → 點目標 — as a pure
// accumulator. The shell feeds hits; the model decides when a
// command is complete and dispatches the matching Issue* call.
// Same convention as MarchIntent: a click is intent, the commit
// is the controller's own gate — this never re-derives rules
// (CP, legality), it only sequences input.

// What a verb still needs after its source squad is picked.
enum class VerbTarget : std::uint8_t {
    None,       // Retreat — the squad IS the command
    Region,     // Redirect, Probe, MythPacify, MythGhostArmy
    OwnSquad,   // MythPossession
    Slot,       // Override — the sheet slot to force
    Path,       // Replan — click a region chain, then commit
    EnemyCloud, // Execute, Subvert — cloud resolves to the
                // victim via SquadForCloud (belief handle, not
                // truth dereference)
    CloudPair,  // Entangle — two clouds share fate
};

// Which verbs consume a source squad (the first click).
// Probe/Entangle/Execute/Subvert/myth acts are squad-less —
// the grammar still lets a squad sit selected, it just isn't
// required.
constexpr bool NeedsSquad(Gameplay::InterventionKind k) {
    switch (k) {
        case Gameplay::InterventionKind::Redirect:
        case Gameplay::InterventionKind::Override:
        case Gameplay::InterventionKind::Retreat:
        case Gameplay::InterventionKind::Replan:
        case Gameplay::InterventionKind::MythPossession:
            return true;
        default:
            return false;
    }
}

constexpr VerbTarget TargetOf(Gameplay::InterventionKind k) {
    switch (k) {
        case Gameplay::InterventionKind::Retreat:
            return VerbTarget::None;
        case Gameplay::InterventionKind::Redirect:
        case Gameplay::InterventionKind::Probe:
        case Gameplay::InterventionKind::MythPacify:
        case Gameplay::InterventionKind::MythGhostArmy:
            return VerbTarget::Region;
        case Gameplay::InterventionKind::MythPossession:
            return VerbTarget::OwnSquad;
        case Gameplay::InterventionKind::Override:
            return VerbTarget::Slot;
        case Gameplay::InterventionKind::Replan:
            return VerbTarget::Path;
        case Gameplay::InterventionKind::Execute:
        case Gameplay::InterventionKind::Subvert:
            return VerbTarget::EnemyCloud;
        case Gameplay::InterventionKind::Entangle:
            return VerbTarget::CloudPair;
    }
    return VerbTarget::None;
}

// A map click resolves to a cloud only through belief: the
// most certain cloud believed at `region` (lowest id breaks
// ties — canonical order). -1 = nothing believed there. The
// caller hands the id to BattleIntent::FeedCloud, which the
// controller resolves to truth itself.
inline int CloudAtRegion(const Gameplay::QuantumFog& fog,
                         std::size_t region) {
    int best = -1, bestCertainty = -1;
    for (const auto& c : fog.Clouds()) {
        if (c.believedRegion != static_cast<int>(region)) {
            continue;
        }
        if (c.certainty > bestCertainty ||
            (c.certainty == bestCertainty && best < 0)) {
            bestCertainty = c.certainty;
            best = c.id;
        }
    }
    return best;
}

class BattleIntent {
public:
    int Squad() const { return squad_; }
    // -1 = nothing armed; else the InterventionKind ordinal.
    int Verb() const { return verb_; }
    bool Armed() const { return verb_ >= 0; }
    int FirstCloud() const { return firstCloud_; }
    const std::vector<std::size_t>& Path() const {
        return path_;
    }

    void Clear() { *this = BattleIntent{}; }

    // The source pick — clears any armed verb (a new hand needs
    // a fresh card).
    void SelectSquad(int idx) {
        squad_ = idx;
        verb_ = -1;
        firstCloud_ = -1;
        region_ = -1;
        slot_ = -1;
        victim_ = -1;
        secondCloud_ = -1;
        path_.clear();
    }
    // Arm/toggle a verb. Re-arming the same verb disarms.
    // Returns the post-state: true = armed.
    bool Arm(Gameplay::InterventionKind k) {
        const int v = static_cast<int>(k);
        if (verb_ == v) {
            verb_ = -1;
            return false;
        }
        verb_ = v;
        firstCloud_ = -1;
        region_ = -1;
        slot_ = -1;
        victim_ = -1;
        secondCloud_ = -1;
        return true;
    }

    // Path accumulation — shared by Replan (Execution) and the
    // Planning arrow pass (the shell seeds with the squad's
    // current region, then each legal adjacent click extends).
    void BeginPath(std::size_t region) {
        path_.clear();
        path_.push_back(region);
    }
    // Appends when `region` is adjacent to the path tail;
    // returns false otherwise. Re-clicking the tail truncates
    // (walk-back editing).
    bool ExtendPath(const Gameplay::BattleMap& map,
                    std::size_t region);

    // Target feeds — true when the intent is complete and
    // Ready(). The cloud feeds resolve belief handles through
    // the controller's own mapping.
    bool FeedRegion(std::size_t region) {
        if (TargetOf(Verb()) == VerbTarget::Path) {
            return false; // paths commit via Fire, not a click
        }
        region_ = static_cast<int>(region);
        return true;
    }
    bool FeedCloud(const Gameplay::BattleController& bc,
                   int side, int cloudId);
    bool FeedSlot(int slot) {
        slot_ = slot;
        return true;
    }
    bool FeedOwnSquad(int idx) {
        squad2_ = idx;
        return true;
    }

    // The intent has everything the armed verb needs.
    bool Ready() const;

    // Dispatch through the controller's own Issue* gates —
    // rejections propagate (insufficient CP, illegal target)
    // and the intent clears on ANY terminal dispatch attempt.
    Gameplay::Result<bool> Fire(Gameplay::BattleController& bc,
                                int side);

    // Convenience: the armed verb's kind, or -1.
    Gameplay::InterventionKind VerbKind() const {
        return verb_ < 0
                   ? Gameplay::InterventionKind::Redirect
                   : static_cast<Gameplay::InterventionKind>(verb_);
    }

private:
    static VerbTarget TargetOf(int verb) {
        return verb < 0 ? VerbTarget::None
                        : Game::TargetOf(
                              static_cast<
                                  Gameplay::InterventionKind>(
                                  verb));
    }

    int squad_ = -1;
    int verb_ = -1;
    int region_ = -1;
    int slot_ = -1;
    int victim_ = -1;      // resolved enemy squad (Execute/Subvert)
    int firstCloud_ = -1;
    int secondCloud_ = -1;
    int squad2_ = -1;      // OwnSquad target (possession)
    std::vector<std::size_t> path_;
};

} // namespace Potato::Game
