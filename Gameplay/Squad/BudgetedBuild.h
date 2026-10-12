#pragma once

#include "Gameplay/Squad/Squad.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Potato::Gameplay {

// Story 9.2 — budgeted squad builds. Given a build list of
// template ids and a 物資 budget, walk the list in order:
// affordable items instantiate, refused items record a reason.
// No silent drops — every requested id lands either a Squad or
// a BuildSkip. Greedy in order, deterministic: a too-expensive
// item skips but a later cheap one can still fit what remains.

enum class BuildSkipReason : std::uint8_t {
    UnknownId = 0, // template id not in the library
    OverBudget,    // cost exceeds remaining budget
};

// Stable ASCII wire ids: "unknown_id", "over_budget".
const char* BuildSkipReasonName(BuildSkipReason r);

struct BuildSkip {
    std::string id;
    BuildSkipReason reason = BuildSkipReason::UnknownId;
};

struct BudgetedBuildResult {
    std::vector<Squad> squads;    // instantiated, list order
    // The template each squad was built from — parallel to
    // `squads`, pointers into the library (valid for its
    // lifetime). Deployment callers need the template, not the
    // instance: BattleController::DeploySquad /
    // BattleRecorder::RecordDeploy take SquadTemplate.
    std::vector<const SquadTemplate*> templates;
    std::vector<BuildSkip> skips; // refused items, list order
    int spent = 0;
};

// `budget < 0` behaves as 0. Cost-0 templates always fit.
// All spawned squads share `regionIndex`/`side` (a build list
// is one army's roster landing at one point — multi-region
// deployment is the caller's loop). Duplicate template ids are
// legal — the second and later instances get a deterministic
// `id#n` suffix so battle identity stays unique.
BudgetedBuildResult BuildSquads(const SquadTemplateLibrary& lib,
                                const std::vector<std::string>& ids,
                                int budget, std::size_t regionIndex,
                                int side = 0);

} // namespace Potato::Gameplay
