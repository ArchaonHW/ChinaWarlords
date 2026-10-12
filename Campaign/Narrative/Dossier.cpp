#include "Campaign/Narrative/Dossier.h"

#include "Gameplay/Doctrine/Doctrine.h" // TriggerKind

#include <algorithm>
#include <cstddef>

namespace Potato::Campaign {

namespace {

using Gameplay::TriggerKind;

// Hearsay markers rotate by claim index — every sentence is
// attributed to rumor, none to the archive. Deterministic pick.
const char* kMarkers[] = {"據聞", "或曰", "傳言"};

std::string_view TemperamentText(RivalPrior p) {
    switch (p) {
        case RivalPrior::Aggressive:
            return "其人性如烈火，好疾戰而惡持久";
        case RivalPrior::Defensive:
            return "其人持重，善守不浪戰";
        case RivalPrior::Cunning:
            return "其人詭譎，言行不可憑";
    }
    return "其人未詳";
}

// What their scouts are rumored to have concluded about OUR
// doctrine — the trigger histogram is the rival's read on the
// player, so the dossier says what the enemy believes.
std::string_view HabitText(TriggerKind t) {
    switch (t) {
        case TriggerKind::Always:
            return "其斥候謂我軍動輒施令，無有常形";
        case TriggerKind::CohesionBelow:
            return "其斥候謂我軍怯於挫鋒，遇損即斂";
        case TriggerKind::EnemyInRegion:
            return "其斥候謂我軍見敵入境必應";
        case TriggerKind::EnemyAdjacent:
            return "其斥候謂我軍聞鄰警即動";
    }
    return "其斥候語焉不詳";
}

// The dominant learned habit — highest count wins, ties fall to
// lowest ordinal (deterministic). Returns false if nothing was
// ever observed.
bool DominantTrigger(const TriggerHistogram& h, TriggerKind& out) {
    std::size_t best = 0;
    bool any = false;
    for (std::size_t i = 0; i < h.size(); ++i) {
        if (h[i] > 0 && (!any || h[i] > h[best])) {
            best = i;
            any = true;
        }
    }
    if (!any) return false;
    out = static_cast<TriggerKind>(best);
    return true;
}

} // namespace

std::vector<DossierClaim> DossierClaims(const GeneralDossier& d) {
    const int tier =
        d.chaptersObserved >= RivalBook::MIN_CHAPTERS_TO_LEARN ? 1
                                                             : 0;
    std::vector<DossierClaim> claims;
    if (d.chaptersObserved <= 0) {
        claims.push_back({ClaimKind::Repute,
                          "其人未詳，僅聞其名", 0, false});
        return claims;
    }
    claims.push_back({ClaimKind::Temperament,
                      std::string(TemperamentText(d.prior)), tier,
                      false});
    TriggerKind dominant = TriggerKind::Always;
    if (DominantTrigger(d.triggers, dominant)) {
        claims.push_back({ClaimKind::Habit,
                          std::string(HabitText(dominant)), tier,
                          false});
    }
    claims.push_back({ClaimKind::Repute,
                      tier ? "其名久聞於軍中，言人人殊"
                           : "其名始聞，蹤跡未明",
                      tier, false});
    return claims;
}

std::string RenderDossier(const GeneralDossier& d,
                          std::span<const ClaimKind> provenWrong) {
    std::string s = "聞冊·";
    s += d.id;
    s += "\n";
    const auto claims = DossierClaims(d);
    for (std::size_t i = 0; i < claims.size(); ++i) {
        const DossierClaim& c = claims[i];
        const bool revised =
            std::find(provenWrong.begin(), provenWrong.end(),
                      c.kind) != provenWrong.end();
        s += "  ";
        s += kMarkers[i % 3];
        s += c.text;
        s += "。";
        s += (c.tier > 0) ? "（屢聞）" : "（風聞未確）";
        if (revised) {
            // The dossier confesses the correction inline — the
            // rumor is annotated, never erased.
            s += "——然近事駁之。";
        }
        s += "\n";
    }
    return s;
}

} // namespace Potato::Campaign
