#pragma once

#include "Gameplay/Result.h"
#include "Gameplay/Sim/Sim.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Potato::Gameplay {

// CP economy (GDD FR3): start 3, +1 per 60 s, cap 5.
constexpr int CP_START = 3;
constexpr int CP_CAP = 5;
constexpr int CP_REGEN_TICKS = 60 * TICK_RATE_HZ;

// Mid-Execution player/AI commands. Costs: redirect 1, override 2,
// retreat 3, probe 0 (budgeted separately), entangle 2, replan 2
// (live value from PlanConfig), execute 0 (cruelty is free — the
// ledger is the price). Queued at issue, applied at next tick
// start (well inside the ~3 s resolution budget). Wire-format
// ordinals are append-only.
enum class InterventionKind : std::uint8_t {
    Redirect = 0, Override, Retreat,
    Probe,     // fog op: region intel +probe_gain (no collapse)
    Entangle,  // fog op: link two clouds to share fate
    Replan,    // plan op: rewrite a squad's arrow mid-execution
    Execute,   // refuse a routing enemy's surrender (Epic 4)
    // Myth actions (Epic 5.4) — 0 CP: they are paid in 天命 through
    // the campaign ledger (SpendMandate) BEFORE the verb is issued;
    // an unpaid act shows in the record as an audit finding, not a
    // sim rejection — the same rule Execute set for cruelty.
    MythPacify,     // target = region; backs infiltration off and
                    // releases the shrine's allegiance latch
    MythPossession, // squadIndex = own squad to possess
    MythGhostArmy,  // target = region; raises a spirit garrison
};

// Probes are limited per battle (GDD: ~2-3), not by CP.
constexpr int PROBE_BUDGET = 3;

constexpr int CostOf(InterventionKind k) {
    switch (k) { // no default: new kinds must pick a cost
        case InterventionKind::Redirect: return 1;
        case InterventionKind::Override: return 2;
        case InterventionKind::Retreat:  return 3;
        case InterventionKind::Probe:    return 0;
        case InterventionKind::Entangle: return 2;
        case InterventionKind::Replan:   return 2; // default; live cost
                                                 // comes from PlanConfig
        case InterventionKind::Execute:  return 0;
        case InterventionKind::MythPacify:
        case InterventionKind::MythPossession:
        case InterventionKind::MythGhostArmy: return 0; // paid in 天命
    }
    return 0; // unreachable — all enumerators handled
}

// target: region index (Redirect, Probe) or slot index (Override) or
// cloud id B (Entangle — squadIndex field carries cloud id A) or
// path length (Replan) or the victim's region (Execute — squadIndex
// carries the victim; apply re-derives the region, target is
// journal/checksum only); unused for Retreat. `path` is Replan's
// payload (empty for the rest).
// issueTick is recorded for replay/ordering metadata — the apply step
// doesn't re-read it (queue order is canonical).
struct Intervention {
    int side = 0;
    InterventionKind kind = InterventionKind::Redirect;
    int squadIndex = -1;
    int target = -1;
    int issueTick = 0;
    std::vector<std::size_t> path;
};

} // namespace Potato::Gameplay
