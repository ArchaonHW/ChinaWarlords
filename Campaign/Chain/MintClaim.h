#pragma once

#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Potato::Campaign {

// Story 11.1 — the mint-claim container (Epic MT, D-ARCH-9).
// A claim is the only artifact that crosses the chain boundary:
// versioned JSON written into an outbox directory, consumed by the
// external relayer (Story 11.4). The doc is content-bound — it
// carries the ledger seal (count+tip) and the battle record root it
// claims credit for, so the relayer can re-verify against the
// ledger file itself. Deterministic id derived from content:
// re-emitting the same claim yields the same file, matching
// ResolveAftermath's record_root idempotency.
//
// Wire shape (one file per claim):
//   {"schema":"potato.mintclaim/1","id":"settlement-0123abcd…",
//    "kind":"chapter_settlement","chapter":7,
//    "record_root":"0123abcdef456789","resolution":"governance_victory",
//    "ledger":{"count":412,"tip":-2606419513542532627}}
//   achievement variant:
//   {"schema":"potato.mintclaim/1","id":"achievement-zero_combat-0007",
//    "kind":"achievement","chapter":7,"achievement":"zero_combat",
//    "ledger":{"count":412,"tip":-2606419513542532627}}
//   settlement variant (Story 12.5 — a world encounter's verdict):
//   {"schema":"potato.mintclaim/1","id":"settlement-0123abcd…",
//    "kind":"settlement","encounter":"longmen_ambush",
//    "node":"longmen","record_root":"0123abcdef456789",
//    "resolution":"battle_victory",
//    "ledger":{"count":412,"tip":-2606419513542532627}}
// Ids use '-' separators because the id IS the filename
// (<outbox>/<id>.json) and ':' is illegal in Windows filenames;
// achievement keys are likewise restricted to [a-z0-9_].
// Hash-family uint64s cross the wire as int64 bit-casts inside
// `ledger` (Ledger.h wire note); record_root travels as 16-char
// lowercase hex — the CrossCheck record_root spelling — which also
// dodges the >=2^63 Real-parse trap.

enum class ClaimKind : std::uint8_t {
    ChapterSettlement, // "chapter_settlement"
    Achievement,       // "achievement"
    Settlement,        // "settlement" — world encounter (12.5)
};
const char* ClaimKindName(ClaimKind k);
bool ClaimKindFromName(std::string_view name, ClaimKind& out);

struct MintClaim {
    static constexpr std::string_view SCHEMA = "potato.mintclaim/1";
    static constexpr std::size_t MAX_KEY_LEN = 64; // achievement key
    static constexpr std::size_t MAX_ID_LEN = 128;

    ClaimKind kind = ClaimKind::ChapterSettlement;
    std::int64_t chapter = 0;     // chapter_settlement/achievement
    std::uint64_t recordRoot = 0; // settlement kinds: the record's
                                  // integrity root (BattleRecorder)
    std::string resolution;       // settlement kinds: ResolutionName
                                  // spelling (+ "subversion", Epic 7)
    std::string achievement;      // achievement only: trigger key
    std::string encounter;        // "settlement" only: encounter id
    std::string node;             // "settlement" only: world region
    std::uint64_t ledgerCount = 0; // Ledger::Size() at seal time
    std::uint64_t ledgerTip = 0;   // Ledger::Tip() at seal time

    Gameplay::Result<Gameplay::JsonValue> ToJson() const;
    static Gameplay::Result<MintClaim> FromJson(
        const Gameplay::JsonValue& doc);
};

// Deterministic, content-derived — settlement ids bind the battle
// record root ("settlement-<hex>"); achievement ids bind key and
// chapter ("achievement-<key>-<4-digit chapter>"). Re-emitting the
// same claim yields the same id, hence the same outbox filename.
std::string ClaimId(const MintClaim& c);

// 16-char lowercase hex — the CrossCheck record_root spelling.
std::string RootHex(std::uint64_t root);
bool ParseRootHex(std::string_view s, std::uint64_t& out);

// Bounded resolution spellings a settlement claim may carry. The
// three ChapterResolution names (Victory.h) plus "subversion" —
// Epic 7's negotiated outcomes seal through the same machinery.
bool IsResolutionName(std::string_view name);

} // namespace Potato::Campaign
