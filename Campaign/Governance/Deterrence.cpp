#include "Campaign/Governance/Deterrence.h"

#include "Campaign/Chapters/ChapterLibrary.h"
#include "Campaign/Chapters/Progression.h"
#include "Campaign/Ledger/Ledger.h"
#include "Campaign/State/CampaignState.h"

namespace Potato::Campaign {

bool DeterrenceMet(const Ledger& l) {
    return l.Balance(Account::ArmyPrestige) >= DETERRENCE_PRESTIGE;
}

Gameplay::Result<ChapterResolution> ResolveZeroCombatChapter(
    CampaignState& state, const ChapterLibrary& lib) {
    // Same gate list as the seal — delegation, not duplication.
    std::string_view error;
    if (const char* why = CanResolveChapter(state, lib, error)) {
        return Gameplay::Fail<ChapterResolution>(std::string(error),
                                                 why);
    }
    const ChapterDef* def =
        lib.AtIndex(state.GetChapter().current);
    if (def == nullptr) { // unreachable — the gate just proved it
        return Gameplay::Fail<ChapterResolution>(
            "state", "current is not a registered chapter");
    }
    if (def->combat) {
        return Gameplay::Fail<ChapterResolution>(
            "chapter", "combat chapter requires a field resolution");
    }
    // The enemy yields to standing, not to a deadline — thin
    // prestige means the parley simply hasn't landed yet.
    if (!DeterrenceMet(state.GetLedger())) {
        return Gameplay::Fail<ChapterResolution>(
            "deterrence", "standing too thin to yield the field");
    }
    return ConcludeChapter(state, lib, /*battleWon=*/true,
                           /*recordRoot=*/0, /*subverted=*/true);
}

} // namespace Potato::Campaign
