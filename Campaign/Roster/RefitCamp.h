#pragma once

#include "Campaign/State/CampaignState.h"
#include "Gameplay/Result.h"

#include <cstdint>
#include <string>

namespace Potato::Campaign {

// RefitCamp (Story 3.6): between-chapter actions priced from the
// GDD economy table by band. Every action is atomic — all checks
// pass before any posting or mutation lands; a rejected action
// leaves the ledger and roster untouched and names its skip
// reason in Result::reason.
//
// Spending posts debit Materiel / credit ArmyPrestige: materiel
// converts into army standing, same asymmetric-posting model as
// battle events. Overspend — Balance(Materiel) < cost — rejects
// BEFORE the posting.

struct RefitCosts {
    std::int64_t heal = 0;
    std::int64_t recruit = 0;
    std::int64_t plunderGain = 0;  // +物資
    std::int64_t plunderCost = 0;  // −民心
};

// Economy band by chapter index (GDD tempo table):
//   early 0-2:  heal 20, recruit 40, plunder +30/-10
//   mid   3-7:  heal 30, recruit 60, plunder +45/-15
//   late  8+:   heal 40, recruit 80, plunder +60/-20
RefitCosts BandCosts(std::int64_t chapterIndex);

// Result<int> semantics differ per call — Deploy/Heal/Recruit
// return the roster index; Plunder returns the ledger seq.
//
// Affordability reads Balance(Materiel), which is spendable-truth
// — forged entries fold into it identically to honest income
// (2.3's contract: suspicion is data, not exclusion).

// Deploy is free — validates the squad exists and is alive.
// Selection itself is phase bookkeeping; the caller tracks the
// deployed set.
Gameplay::Result<int> Deploy(CampaignState& state,
                             const std::string& name);

// Clears the squad's accumulated casualties to 0 (replacements
// absorb losses) for the band heal cost. Dead squads cannot be
// healed — `dead` is permanent (3.5 memorial semantics); the
// ledger is the scar, not the counter. Fails on unknown name,
// dead squad, full-strength squad, or insufficient 物資.
Gameplay::Result<int> Heal(CampaignState& state,
                           const std::string& name,
                           std::int64_t chapterIndex);

// Enlists a new named squad for the band recruit cost. Fails on
// Enlist's name/roster invariants or insufficient 物資.
Gameplay::Result<int> Recruit(CampaignState& state,
                              const std::string& name,
                              std::int64_t chapterIndex);

// No roster effect: posts the plunder pair — credit Materiel /
// debit PopularSupport (looting feeds the army and starves the
// countryside).
Gameplay::Result<int> Plunder(CampaignState& state,
                              std::int64_t chapterIndex);

} // namespace Potato::Campaign
