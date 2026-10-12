#include "Campaign/Aftermath/Aftermath.h"

#include "Campaign/Chapters/Progression.h"
#include "Campaign/Ledger/CrossCheck.h"
#include "Campaign/Ledger/DeedBook.h"
#include "Campaign/Myth/MythLog.h"

#include <string>
#include <string_view>

namespace Potato::Campaign {

bool SettlementRecorded(const Ledger& l, std::uint64_t root) {
    const std::string anchor = RecordRootTag(root);
    for (const LedgerEntry& e : l.Entries()) {
        for (const std::string& t : e.tags) {
            if (t == anchor) return true;
        }
    }
    return false;
}

Gameplay::Result<ChapterSettlement> ResolveAftermath(
    CampaignState& state, const ChapterLibrary& lib, bool battleWon,
    int playerSide,
    std::span<const Gameplay::SimEvent> deeds,
    const std::vector<AftermathRow>& casualties,
    std::uint64_t recordRoot,
    MythLog* mythLog) {
    // Preflight before ANY mutation — every mutating stage below is
    // unfailable once these pass.
    //  - the battle must carry its record anchor (and must not
    //    already be settled — retry safety)
    //  - the side must be valid (BookDeeds would otherwise reject
    //    AFTER the roster mutated — an unretryable partial land)
    //  - the chapter must be able to close (shared gate)
    //  - the ledger needs worst-case room (every event + the seal)
    //  - the MythLog needs room for every invasion it would fold
    if (recordRoot == 0) {
        return Gameplay::Fail<ChapterSettlement>(
            "aftermath", "settlement requires the record root");
    }
    if (SettlementRecorded(state.GetLedger(), recordRoot)) {
        return Gameplay::Fail<ChapterSettlement>(
            "aftermath", "battle already settled");
    }
    if (playerSide != 0 && playerSide != 1) {
        return Gameplay::Fail<ChapterSettlement>(
            "deeds", "side must be 0 or 1");
    }
    std::string_view error;
    if (const char* why = CanResolveChapter(state, lib, error)) {
        return Gameplay::Fail<ChapterSettlement>(std::string(error),
                                                 why);
    }
    if (deeds.size() >= Ledger::MAX_ENTRIES ||
        state.GetLedger().Size() + deeds.size() + 1 >
            Ledger::MAX_ENTRIES) {
        return Gameplay::Fail<ChapterSettlement>(
            "posting", "ledger lacks capacity for settlement");
    }
    if (mythLog != nullptr) {
        std::size_t invasions = 0;
        for (const Gameplay::SimEvent& e : deeds) {
            if (e.kind == Gameplay::SimEvent::Kind::MythInvasion) {
                ++invasions;
            }
        }
        if (mythLog->Size() + invasions > MythLog::MAX_ENTRIES) {
            return Gameplay::Fail<ChapterSettlement>(
                "mythlog", "log lacks capacity for settlement");
        }
    }

    // 1. Casualties are the irrevocable fact — persist first.
    //    Atomic internally: a rejected report leaves everything
    //    (roster AND ledger) untouched.
    const auto roster = ApplyAftermath(state, casualties);
    if (!roster.ok()) {
        return Gameplay::Fail<ChapterSettlement>(roster.error,
                                                 roster.reason);
    }

    // 2. Deeds book while the fold can still see them.
    const auto posted =
        BookDeeds(state.GetLedger(), playerSide, deeds);
    if (!posted.ok()) {
        return Gameplay::Fail<ChapterSettlement>(posted.error,
                                                 posted.reason);
    }

    // 3. Chronicle the visitations before the seal — the MythLog
    //    fold is preflighted for capacity, so a mid-fold failure is
    //    unreachable; it sits ahead of ConcludeChapter because a
    //    Fail AFTER the verdict would be a lie (chapter already
    //    advanced, retry already rejected by the record anchor).
    std::size_t mythLogged = 0;
    if (mythLog != nullptr) {
        const auto logged = LogMythEvents(*mythLog, deeds);
        if (!logged.ok()) {
            return Gameplay::Fail<ChapterSettlement>(logged.error,
                                                     logged.reason);
        }
        mythLogged = logged.value;
    }

    // 4. Seal the verdict — ConcludeChapter folds (deeds inside),
    //    posts the resolution anchored to the battle record,
    //    advances. Preflighted to succeed.
    const auto res =
        ConcludeChapter(state, lib, battleWon, recordRoot);
    if (!res.ok()) {
        return Gameplay::Fail<ChapterSettlement>(res.error,
                                                 res.reason);
    }

    ChapterSettlement s;
    s.resolution = res.value;
    s.roster = roster.value;
    s.deedsPosted = posted.value;
    s.mythLogged = mythLogged;
    s.nextChapter = static_cast<int>(state.GetChapter().current);
    return Gameplay::Ok(s);
}

} // namespace Potato::Campaign
