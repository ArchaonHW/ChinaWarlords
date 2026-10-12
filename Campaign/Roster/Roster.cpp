#include "Campaign/Roster/Roster.h"

#include <string>
#include <unordered_set>

namespace Potato::Campaign {

namespace {

Gameplay::Result<int> FindRoster(const CampaignState& state,
                                 const std::string& name) {
    const auto& roster = state.GetRoster();
    for (std::size_t i = 0; i < roster.size(); ++i) {
        if (roster[i].name == name) {
            return Gameplay::Ok(static_cast<int>(i));
        }
    }
    return Gameplay::Fail<int>(
        "roster", "unknown squad name");
}

} // namespace

const char* CanEnlist(const CampaignState& state,
                      const std::string& name) {
    if (name.empty() ||
        name.size() > CampaignState::MAX_NAME_LEN) {
        return "bad name";
    }
    if (state.GetRoster().size() >= CampaignState::MAX_ROSTER) {
        return "roster full";
    }
    if (FindRoster(state, name).ok()) {
        return "duplicate name";
    }
    return nullptr;
}

Gameplay::Result<int> Enlist(CampaignState& state,
                             const std::string& name) {
    if (const char* why = CanEnlist(state, name)) {
        return Gameplay::Fail<int>("roster", why);
    }
    auto& roster = state.GetRoster();
    roster.push_back(RosterEntry{name, 0, 0, false});
    return Gameplay::Ok(static_cast<int>(roster.size() - 1));
}

Gameplay::Result<AftermathResult> ApplyAftermath(
    CampaignState& state,
    const std::vector<AftermathRow>& rows) {
    // Every accepted row must name a distinct live squad, so a
    // report bigger than the roster is provably garbage — reject
    // before allocating `applyIdx`.
    if (rows.size() > state.GetRoster().size()) {
        return Gameplay::Fail<AftermathResult>(
            "aftermath", "report larger than roster");
    }

    // Phase 1: validate every row against an unmutated roster.
    // Any failure rejects the entire aftermath — a caller bug or
    // forged report must not half-apply.
    std::vector<int> applyIdx(rows.size());
    std::unordered_set<std::string> seen;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        if (row.casualties < 0 ||
            row.casualties > CampaignState::MAX_ROSTER_COUNT) {
            return Gameplay::Fail<AftermathResult>(
                "aftermath", "casualties out of range");
        }
        if (!seen.insert(row.name).second) {
            return Gameplay::Fail<AftermathResult>(
                "aftermath", "duplicate squad in report");
        }
        const auto found = FindRoster(state, row.name);
        if (!found.ok()) {
            return Gameplay::Fail<AftermathResult>(
                "roster", "unknown squad name");
        }
        const auto& e = state.GetRoster()[
            static_cast<std::size_t>(found.value)];
        if (e.dead) {
            return Gameplay::Fail<AftermathResult>(
                "roster", "dead squad cannot fight");
        }
        // Stored counters may have bypassed Enlist via the
        // mutable GetRoster() accessor — treat them as untrusted
        // and never do arithmetic on them until range-checked.
        if (e.casualties < 0 || e.veterancy < 0 ||
            e.casualties > CampaignState::MAX_ROSTER_COUNT ||
            e.veterancy > CampaignState::MAX_ROSTER_COUNT) {
            return Gameplay::Fail<AftermathResult>(
                "roster", "stored counters out of range");
        }
        if (row.casualties >
            CampaignState::MAX_ROSTER_COUNT - e.casualties) {
            return Gameplay::Fail<AftermathResult>(
                "aftermath", "casualties would overflow bound");
        }
        if (!row.wiped &&
            e.veterancy >= CampaignState::MAX_ROSTER_COUNT) {
            return Gameplay::Fail<AftermathResult>(
                "aftermath", "veterancy would overflow bound");
        }
        applyIdx[i] = found.value;
    }

    // Phase 2: apply.
    AftermathResult res;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto& e = state.GetRoster()[
            static_cast<std::size_t>(applyIdx[i])];
        e.casualties += rows[i].casualties;
        if (rows[i].wiped) {
            e.dead = true;
            ++res.buried;
        } else {
            ++e.veterancy;
            ++res.veterans;
        }
        ++res.rows;
    }
    return Gameplay::Ok(res);
}

} // namespace Potato::Campaign
