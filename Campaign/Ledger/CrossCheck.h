#pragma once

#include "Campaign/Ledger/Ledger.h"
#include "Gameplay/Json/JsonValue.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Campaign {

// The cross-check verdict. RecordInconsistent = the record doc
// fails its OWN integrity seal (declared root doesn't cover the
// payload) — provable corruption independent of coverage, and the
// only state in which anchors can diverge (relevant anchors flag
// suspect — the AC's "chapter state as suspect", surfacing in the
// historian report's omissions and audit segment). A self-
// consistent record's anchors always agree, so there is no
// "clean record, lying ledger" verdict at the value-binding level —
// ledger-side fabrication instead shows up via OrphanAnchors().
// NoAnchor also results from a fully-resigned tampered record
// (payload + root both rewritten consistently) — indistinguishable
// from an uncovered battle.
enum class CrossVerdict : std::uint8_t {
    Clean,        // record self-consistent; every relevant anchor
                  // agrees
    NoAnchor,     // no ledger entry claims this record — absence,
                  // not disagreement (see note above)
    RecordInconsistent, // record fails its own seal; diverging
                  // anchors flagged suspect
    ChainBroken,  // l.Verify() failed (defensive; a live ledger from
                  // Post/Forge/FromJson can't be broken)
    BadRecord,    // record doc malformed: no integrity.root
};

struct CrossCheckResult {
    CrossVerdict verdict = CrossVerdict::BadRecord;
    std::uint64_t anchorSeq = 0;      // first relevant anchor (0 when
                                      // anchors==0)
    std::uint64_t declaredRoot = 0;   // record's integrity.root field
    std::uint64_t recomputedRoot = 0; // root recomputed from payload
    std::uint64_t ledgerRoot = 0;     // first relevant anchor's claim
    std::size_t anchors = 0;          // relevant anchor claims (tags)
    std::size_t flagged = 0;          // entries newly flagged suspect
    std::size_t forgedAnchors = 0;    // relevant claims on forged
                                      // entries — coverage by fiction;
                                      // confessed in reports but
                                      // exposed here too
};

// Tag convention: an entry covers a battle record iff it carries
// "record_root:<16 lowercase hex>" — the record's integrity root
// bound into the hashed chain. Fits inside MAX_TAG_LEN.
std::string RecordRootTag(std::uint64_t root);
bool ParseRecordRootTag(std::string_view tag, std::uint64_t& out);

// Cross-check a battle record doc (potato.battle_record) against
// the ledger chain. Recomputes the record's integrity root
// (BattleRecorder::ComputeRoot over the doc minus `integrity` —
// note: extra members inside `integrity` are hash-exempt, today
// only `root` is defined). Anchor relevance: a stored root equal to
// the declared OR recomputed root — other anchors belong to other
// battles' records and are skipped.
CrossCheckResult CrossCheckRecord(
    Ledger& l, const Gameplay::JsonValue& recordDoc);

// Complementary ledger-side sweep: seqs of entries carrying
// record_root: tags claiming roots none of the presented records
// satisfy. Callers must present the chapter's COMPLETE record set —
// an anchor for an un-presented record looks identical to a
// dangling claim. Query only; the caller decides whether to flag.
std::vector<std::uint64_t> OrphanAnchors(
    const Ledger& l, const std::vector<std::uint64_t>& presentedRoots);

} // namespace Potato::Campaign
