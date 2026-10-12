#include "Campaign/Governance/Victory.h"

#include "Campaign/Chapters/Progression.h"
#include "Campaign/Ledger/CrossCheck.h"

#include <string>
#include <string_view>

namespace Potato::Campaign {
namespace {

// Token seal amount — the resolution lives in the tags; the legs
// exist because double-entry demands a real pair. One grain moves:
// negligible to any balance, honest to the form.
constexpr std::int64_t kSealAmount = 1;

// The verdict's bookkeeping — small and directional:
//   governance victory: 民心一分凝為天命 (support consolidates)
//   battle victory:     軍威以物資記   (glory paid in supply)
//   defeat:             收殮得物資，軍威折損 (salvage; prestige bleeds)
Posting SealPosting(ChapterResolution r, std::int64_t chapter,
                    std::uint64_t recordRoot) {
    Posting p;
    // No `default:` — a new ChapterResolution enumerator trips
    // -Wswitch here (and in ResolutionName), fail-closed by the
    // warning rather than silently sealing wrong legs.
    switch (r) {
    case ChapterResolution::GovernanceVictory:
        p.credit = {Account::Mandate, kSealAmount};
        p.debit = {Account::PopularSupport, kSealAmount};
        break;
    case ChapterResolution::BattleVictory:
        p.credit = {Account::ArmyPrestige, kSealAmount};
        p.debit = {Account::Materiel, kSealAmount};
        break;
    case ChapterResolution::Defeat:
        p.credit = {Account::Materiel, kSealAmount};
        p.debit = {Account::ArmyPrestige, kSealAmount};
        break;
    }
    p.memo = "chapter " + std::to_string(chapter) +
             " resolved: " + ResolutionName(r);
    p.tags = {std::string(Ledger::TAG_RESOLUTION) + ResolutionName(r),
              std::string(Ledger::TAG_CHAPTER) +
                  std::to_string(chapter)};
    // Anchored verdicts cite the battle record's integrity root —
    // tamper-evident and retry-safe (a settled root can't re-seal).
    if (recordRoot != 0) p.tags.push_back(RecordRootTag(recordRoot));
    return p;
}

} // namespace

const char* ResolutionName(ChapterResolution r) {
    switch (r) {
    case ChapterResolution::BattleVictory:
        return "battle_victory";
    case ChapterResolution::GovernanceVictory:
        return "governance_victory";
    case ChapterResolution::Defeat:
        return "defeat";
    }
    return "unknown";
}

bool IsGovernanceVictory(const GovernanceAccumulators& a) {
    return a.popularSupport >= VICTORY_POPULAR_SUPPORT &&
           a.order >= VICTORY_ORDER;
}

Gameplay::Result<ChapterResolution> ConcludeChapter(
    CampaignState& state, const ChapterLibrary& lib, bool battleWon,
    std::uint64_t recordRoot) {
    // Shared gate predicate — same list ResolveAndAdvance enforces,
    // so post-seal advance cannot fail (the two can't drift).
    std::string_view error;
    if (const char* why = CanResolveChapter(state, lib, error)) {
        return Gameplay::Fail<ChapterResolution>(std::string(error),
                                                 why);
    }
    // The second preflight: Post must not be able to fail either.
    if (state.GetLedger().Size() >= Ledger::MAX_ENTRIES) {
        return Gameplay::Fail<ChapterResolution>("posting",
                                                 "ledger at MAX_ENTRIES");
    }

    // The ledger decides — fold first, THEN look at the field.
    // Thresholds met is a governance victory no matter how the last
    // battle went: the people's verdict outranks the casualty list.
    const GovernanceAccumulators a =
        FoldGovernance(state.GetLedger());
    const ChapterResolution r =
        IsGovernanceVictory(a)
            ? ChapterResolution::GovernanceVictory
            : (battleWon ? ChapterResolution::BattleVictory
                         : ChapterResolution::Defeat);

    const std::int64_t chapter = state.GetChapter().current;
    const auto posted =
        state.GetLedger().Post(SealPosting(r, chapter, recordRoot));
    if (!posted.ok()) {
        return Gameplay::Fail<ChapterResolution>(posted.error,
                                                 posted.reason);
    }
    // Guaranteed by the shared CanResolveChapter gate + untouched
    // chapter state since.
    const auto next = ResolveAndAdvance(state, lib);
    if (!next.ok()) {
        return Gameplay::Fail<ChapterResolution>(next.error,
                                                 next.reason);
    }
    return Gameplay::Ok(r);
}

} // namespace Potato::Campaign
