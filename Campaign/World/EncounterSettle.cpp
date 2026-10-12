#include "Campaign/World/EncounterSettle.h"

#include "Campaign/Aftermath/Aftermath.h" // SettlementRecorded
#include "Campaign/Ledger/CrossCheck.h"  // RecordRootTag
#include "Campaign/Ledger/DeedBook.h"    // BookDeeds
#include "Campaign/Myth/MythLog.h"       // LogMythEvents
#include "Campaign/State/CampaignState.h"
#include "Campaign/World/Encounter.h"
#include "Campaign/World/WorldMap.h"
#include "Campaign/World/WorldState.h"
#include "Gameplay/Doctrine/Doctrine.h"  // SimEvent

#include <string>

namespace Potato::Campaign {
namespace {

// Token seal amount — must mirror Victory.cpp's SealPosting legs
// (the three-case table is the same verdict's bookkeeping; the
// memo/tags differ because this seal names an encounter, not a
// chapter).
constexpr std::int64_t kSealAmount = 1;

Posting SealEncounterPosting(ChapterResolution r,
                             const EncounterDef& enc,
                             std::uint64_t recordRoot) {
    Posting p;
    // MUST mirror Victory.cpp SealPosting legs — no default: a new
    // ChapterResolution enumerator trips -Wswitch here and there.
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
    p.memo = "encounter " + enc.id +
             " resolved: " + ResolutionName(r);
    // No chapter: tag — the encounter cites itself and its ground.
    p.tags = {std::string(Ledger::TAG_RESOLUTION) + ResolutionName(r),
              "encounter:" + enc.id,
              std::string(Ledger::TAG_REGION) + enc.node};
    p.tags.push_back(RecordRootTag(recordRoot));
    return p;
}

} // namespace

Gameplay::Result<EncounterSettlement> SettleEncounter(
    CampaignState& state, WorldState& ws, const WorldMap& world,
    const EncounterDef& enc, bool battleWon, int playerSide,
    std::span<const Gameplay::SimEvent> deeds,
    const std::vector<AftermathRow>& casualties,
    std::uint64_t recordRoot, MythLog* mythLog) {
    // Preflight before ANY mutation — same discipline as
    // ResolveAftermath: every mutating stage below is unfailable
    // once these pass.
    if (recordRoot == 0) {
        return Gameplay::Fail<EncounterSettlement>(
            "settle", "settlement requires the record root");
    }
    if (SettlementRecorded(state.GetLedger(), recordRoot)) {
        return Gameplay::Fail<EncounterSettlement>(
            "settle", "battle already settled");
    }
    if (playerSide != 0 && playerSide != 1) {
        return Gameplay::Fail<EncounterSettlement>(
            "deeds", "side must be 0 or 1");
    }
    if (world.FindNode(enc.node) == nullptr) {
        return Gameplay::Fail<EncounterSettlement>(
            "settle", "encounter node not in world");
    }
    // Seal-tag fit: "encounter:" (10) + id and "region:" (7) +
    // node must each land within MAX_TAG_LEN or Post rejects
    // mid-ceremony — a checkable content bound, preflighted.
    if (enc.id.size() + 10 > Ledger::MAX_TAG_LEN ||
        enc.node.size() + 7 > Ledger::MAX_TAG_LEN) {
        return Gameplay::Fail<EncounterSettlement>(
            "settle", "encounter/node too long for seal tags");
    }
    if (deeds.size() >= Ledger::MAX_ENTRIES ||
        state.GetLedger().Size() + deeds.size() + 1 >
            Ledger::MAX_ENTRIES) {
        return Gameplay::Fail<EncounterSettlement>(
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
            return Gameplay::Fail<EncounterSettlement>(
                "mythlog", "log lacks capacity for settlement");
        }
    }
    // World writeback capacity: one control event slot when the
    // stake could flip, and one resolved-set slot when new.
    if (battleWon && enc.hasControlStake &&
        ws.Pending().size() >= WorldState::MAX_EVENTS) {
        return Gameplay::Fail<EncounterSettlement>(
            "world", "event queue full");
    }
    if (!ws.IsResolved(enc.id) &&
        ws.ResolvedCount() >= WorldState::MAX_RESOLVED) {
        return Gameplay::Fail<EncounterSettlement>(
            "world", "resolved set full");
    }

    // 1. Casualties are the irrevocable fact — persist first
    //    (atomic internally, same as 4.5).
    const auto roster = ApplyAftermath(state, casualties);
    if (!roster.ok()) {
        return Gameplay::Fail<EncounterSettlement>(roster.error,
                                                   roster.reason);
    }

    // 2. Deeds book while the fold can still see them — stamped
    //    with the world node so 12.7's regional fold attributes
    //    them to the ground they happened on.
    const auto posted = BookDeeds(state.GetLedger(), playerSide,
                                  deeds, enc.node);
    if (!posted.ok()) {
        return Gameplay::Fail<EncounterSettlement>(posted.error,
                                                   posted.reason);
    }

    // 3. Chronicle the visitations before the seal — capacity is
    //    preflighted; a Fail AFTER the seal would be a lie.
    std::size_t mythLogged = 0;
    if (mythLog != nullptr) {
        const auto logged = LogMythEvents(*mythLog, deeds);
        if (!logged.ok()) {
            return Gameplay::Fail<EncounterSettlement>(
                logged.error, logged.reason);
        }
        mythLogged = logged.value;
    }

    // 4. The ledger decides — fold first (deeds inside), THEN the
    //    field result. Same rule as ConcludeChapter: governance
    //    thresholds override whatever the battle did.
    const GovernanceAccumulators a =
        FoldGovernance(state.GetLedger());
    const ChapterResolution r =
        IsGovernanceVictory(a)
            ? ChapterResolution::GovernanceVictory
            : (battleWon ? ChapterResolution::BattleVictory
                         : ChapterResolution::Defeat);
    const auto sealed = state.GetLedger().Post(
        SealEncounterPosting(r, enc, recordRoot));
    if (!sealed.ok()) {
        return Gameplay::Fail<EncounterSettlement>(sealed.error,
                                                   sealed.reason);
    }

    // 5. World writeback — the resolved marker lands ALWAYS
    //    (a defeat still settles: no refire deadlock, the warband
    //    stays put on the node).
    ws.MarkResolved(enc.id); // capacity preflighted
    EncounterSettlement s;
    s.resolution = r;
    s.roster = roster.value;
    s.deedsPosted = posted.value;
    s.mythLogged = mythLogged;

    if (battleWon && enc.hasControlStake) {
        WorldEvent ev;
        ev.day = ws.Day();
        ev.seq = ws.NextSeq();
        ev.kind = WorldEventKind::SetControl;
        ev.node = enc.node;
        ev.control = enc.controlStake;
        const auto enq = ws.Enqueue(world, ev);
        if (!enq.ok()) {
            return Gameplay::Fail<EncounterSettlement>(enq.error,
                                                       enq.reason);
        }
        const auto drained = ws.ResolveBeats(world, 0);
        if (!drained.ok()) {
            return Gameplay::Fail<EncounterSettlement>(
                drained.error, drained.reason);
        }
        s.controlFlipped =
            (ws.ControlAt(enc.node) == enc.controlStake);
    }
    return Gameplay::Ok(s);
}

} // namespace Potato::Campaign
