#include "Game/Ui/BattleCommands.h"

#include "Gameplay/Map/BattleMap.h"
#include "Gameplay/Sim/BattleController.h"

namespace Potato::Game {

bool BattleIntent::ExtendPath(const Gameplay::BattleMap& map,
                              std::size_t region) {
    if (path_.empty()) {
        path_.push_back(region);
        return true;
    }
    if (path_.back() == region) {
        path_.pop_back(); // walk-back editing
        return true;
    }
    // No revisits — a path that loops is a doctrine lie.
    for (std::size_t r : path_) {
        if (r == region) return false;
    }
    for (std::size_t n : map.Neighbors(path_.back())) {
        if (n == region) {
            path_.push_back(region);
            return true;
        }
    }
    return false;
}

bool BattleIntent::FeedCloud(
    const Gameplay::BattleController& bc, int side,
    int cloudId) {
    switch (TargetOf(verb_)) {
        case VerbTarget::EnemyCloud:
            victim_ = bc.SquadForCloud(side, cloudId);
            return victim_ >= 0;
        case VerbTarget::CloudPair:
            if (firstCloud_ < 0) {
                firstCloud_ = cloudId;
                return false;
            }
            secondCloud_ = cloudId;
            return secondCloud_ != firstCloud_;
        default:
            return false;
    }
}

bool BattleIntent::Ready() const {
    if (verb_ < 0) return false;
    if (NeedsSquad(VerbKind()) && squad_ < 0) return false;
    switch (TargetOf(verb_)) {
        case VerbTarget::None:       return true;
        case VerbTarget::Region:     return region_ >= 0;
        case VerbTarget::OwnSquad:   return squad2_ >= 0;
        case VerbTarget::Slot:       return slot_ >= 0;
        case VerbTarget::Path:       return path_.size() >= 2;
        case VerbTarget::EnemyCloud: return victim_ >= 0;
        case VerbTarget::CloudPair:
            return firstCloud_ >= 0 && secondCloud_ >= 0;
    }
    return false;
}

Gameplay::Result<bool> BattleIntent::Fire(
    Gameplay::BattleController& bc, int side) {
    using Gameplay::InterventionKind;
    const InterventionKind k = VerbKind();
    const int squad = squad_;
    const int region = region_;
    const int slot = slot_;
    const int victim = victim_;
    const int squad2 = squad2_;
    const int a = firstCloud_;
    const int b = secondCloud_;
    const auto path = path_;
    Clear();
    switch (k) {
        case InterventionKind::Redirect:
            return bc.IssueRedirect(
                side, squad, static_cast<std::size_t>(region));
        case InterventionKind::Override:
            return bc.IssueOverride(side, squad, slot);
        case InterventionKind::Retreat:
            return bc.IssueRetreat(side, squad);
        case InterventionKind::Probe:
            return bc.IssueProbe(
                side, static_cast<std::size_t>(region));
        case InterventionKind::Entangle:
            return bc.IssueEntangle(side, a, b);
        case InterventionKind::Replan:
            return bc.IssueReplan(side, squad, path);
        case InterventionKind::Execute:
            return bc.IssueExecute(side, victim);
        case InterventionKind::MythPacify:
            return bc.IssueMythPacify(
                side, static_cast<std::size_t>(region));
        case InterventionKind::MythPossession:
            return bc.IssueMythPossession(side, squad2);
        case InterventionKind::MythGhostArmy:
            return bc.IssueGhostArmy(
                side, static_cast<std::size_t>(region));
        case InterventionKind::Subvert:
            return bc.IssueSubvert(side, victim);
    }
    return Gameplay::Fail<bool>("intent", "no verb armed");
}

} // namespace Potato::Game
