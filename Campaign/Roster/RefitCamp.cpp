#include "Campaign/Roster/RefitCamp.h"

#include "Campaign/Roster/Roster.h"

namespace Potato::Campaign {

RefitCosts BandCosts(std::int64_t chapterIndex) {
    // Negative index is garbage input — clamp to the LATE band
    // (most expensive), never the cheapest: fail-closed pricing.
    if (chapterIndex < 0) chapterIndex = INT64_MAX;
    if (chapterIndex <= 2) return {20, 40, 30, 10};
    if (chapterIndex <= 7) return {30, 60, 45, 15};
    return {40, 80, 60, 20};
}

namespace {

// Finds a roster index or fails — kept local so RefitCamp never
// depends on private helpers.
Gameplay::Result<int> Find(const CampaignState& state,
                           const std::string& name) {
    const auto& roster = state.GetRoster();
    for (std::size_t i = 0; i < roster.size(); ++i) {
        if (roster[i].name == name) {
            return Gameplay::Ok(static_cast<int>(i));
        }
    }
    return Gameplay::Fail<int>("refit", "unknown squad name");
}

std::string Shortfall(std::int64_t cost, std::int64_t have) {
    return "skip: insufficient materiel (need " +
           std::to_string(cost) + ", have " +
           std::to_string(have) + ")";
}

// Posts debit Materiel / credit ArmyPrestige for a refit spend.
// Caller must have gated affordability first.
Gameplay::Result<int> PostSpend(CampaignState& state,
                                std::int64_t cost,
                                const char* memo) {
    Posting p;
    p.credit = {Account::ArmyPrestige, cost};
    p.debit = {Account::Materiel, cost};
    p.memo = memo;
    const auto r = state.GetLedger().Post(p);
    if (!r.ok()) {
        return Gameplay::Fail<int>(r.error, r.reason);
    }
    return Gameplay::Ok(static_cast<int>(r.value));
}

} // namespace

Gameplay::Result<int> Deploy(CampaignState& state,
                             const std::string& name) {
    const auto found = Find(state, name);
    if (!found.ok()) return found;
    if (state.GetRoster()[static_cast<std::size_t>(found.value)]
            .dead) {
        return Gameplay::Fail<int>(
            "refit", "skip: dead squad cannot deploy");
    }
    return found;
}

Gameplay::Result<int> Heal(CampaignState& state,
                           const std::string& name,
                           std::int64_t chapterIndex) {
    const auto found = Find(state, name);
    if (!found.ok()) return found;
    const std::size_t idx =
        static_cast<std::size_t>(found.value);
    const auto& e = state.GetRoster()[idx];
    if (e.dead) {
        return Gameplay::Fail<int>(
            "refit", "skip: dead squad cannot heal");
    }
    // Bypassed writes (mutable GetRoster()) can corrupt counters —
    // same untrusted-operand posture as ApplyAftermath.
    if (e.casualties < 0) {
        return Gameplay::Fail<int>(
            "roster", "stored counters out of range");
    }
    if (e.casualties == 0) {
        return Gameplay::Fail<int>(
            "refit", "skip: squad already at full strength");
    }
    const std::int64_t cost = BandCosts(chapterIndex).heal;
    const std::int64_t have =
        state.GetLedger().Balance(Account::Materiel);
    if (have < cost) {
        return Gameplay::Fail<int>("refit",
                                   Shortfall(cost, have));
    }
    const auto posted = PostSpend(state, cost, "refit heal");
    if (!posted.ok()) return posted;
    state.GetRoster()[idx].casualties = 0;
    return found;
}

Gameplay::Result<int> Recruit(CampaignState& state,
                              const std::string& name,
                              std::int64_t chapterIndex) {
    // Gate BOTH sides before any mutation: CanEnlist covers every
    // Enlist failure mode, the ledger-full check covers the only
    // Post failure reachable with a construction-valid posting.
    // After the gates, PostSpend and Enlist are both infallible —
    // atomicity is structural, not a mirrored check-list.
    if (const char* why = CanEnlist(state, name)) {
        return Gameplay::Fail<int>("refit", why);
    }
    if (state.GetLedger().Size() >= Ledger::MAX_ENTRIES) {
        return Gameplay::Fail<int>("refit", "ledger full");
    }
    const std::int64_t cost = BandCosts(chapterIndex).recruit;
    const std::int64_t have =
        state.GetLedger().Balance(Account::Materiel);
    if (have < cost) {
        return Gameplay::Fail<int>("refit",
                                   Shortfall(cost, have));
    }
    const auto posted =
        PostSpend(state, cost, "refit recruit");
    if (!posted.ok()) return posted;
    return Enlist(state, name);
}

Gameplay::Result<int> Plunder(CampaignState& state,
                              std::int64_t chapterIndex) {
    const RefitCosts c = BandCosts(chapterIndex);
    Posting p;
    p.credit = {Account::Materiel, c.plunderGain};
    p.debit = {Account::PopularSupport, c.plunderCost};
    p.memo = "refit plunder";
    // Fold key so the Epic 4 derived accumulators see refit-phase
    // looting — untagged events are invisible to the fold.
    p.tags = {"plunder"};
    const auto r = state.GetLedger().Post(p);
    if (!r.ok()) {
        return Gameplay::Fail<int>(r.error, r.reason);
    }
    return Gameplay::Ok(static_cast<int>(r.value));
}

} // namespace Potato::Campaign
