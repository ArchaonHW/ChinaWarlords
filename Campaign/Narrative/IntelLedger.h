#pragma once

#include "Campaign/Narrative/Dossier.h" // ClaimKind
#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Campaign {

// Story 6.4 — the memory of being wrong. Every piece of
// intelligence enters as a claim and leaves a verdict: intel is a
// narrative device with a lifecycle, not a fact pipe. The verdict
// history is what makes "the dossier lies" a mechanical event
// instead of a theme.

enum class IntelVerdict : std::uint8_t {
    Unresolved = 0, // claim on file, verdict pending
    True = 1,       // later events confirmed it
    False = 2,      // later events refuted it
};
const char* IntelVerdictName(IntelVerdict v);
bool IntelVerdictFromName(std::string_view name, IntelVerdict& out);

// What kind of assertion was made — the feed taxonomy. Rival-
// keyed kinds map to Dossier claim kinds for the proven-wrong
// seam; the others feed fog priors at briefing.
enum class IntelKind : std::uint8_t {
    RivalTemperament = 0, // "the fox is cautious" -> ClaimKind::Temperament
    RivalHabit = 1,       // "their scouts think we press" -> Habit
    EnemyDisposition = 2, // "enemy massing at the ford" -> fog seed
    TerrainIntel = 3,     // "the eastern road is passable" -> fog seed
};
const char* IntelKindName(IntelKind k);
bool IntelKindFromName(std::string_view name, IntelKind& out);

struct IntelEntry {
    std::uint64_t seq = 0;
    std::string subject;  // rival id, region ref, doctrine target
    IntelKind kind = IntelKind::EnemyDisposition;
    std::string claim;    // the assertion as recorded
    std::int64_t chapter = -1;
    IntelVerdict verdict = IntelVerdict::Unresolved;
};

class IntelLedger {
public:
    static constexpr std::string_view SCHEMA = "potato.intel/1";
    static constexpr std::size_t MAX_ENTRIES = 4096;
    static constexpr std::size_t MAX_SUBJECT_LEN = 64;
    static constexpr std::size_t MAX_CLAIM_LEN = 256;

    std::size_t Size() const { return entries_.size(); }
    const std::vector<IntelEntry>& Entries() const {
        return entries_;
    }
    const IntelEntry* Entry(std::uint64_t seq) const;

    // Append a claim; returns its seq. Subject/claim bounds
    // enforced; seqs are dense from 1.
    Gameplay::Result<std::uint64_t> Record(
        std::string_view subject, IntelKind kind,
        std::string_view claim, std::int64_t chapter);

    // Verdicts are terminal — Unresolved -> True|False, never
    // revised again (a reversed verdict is a NEW entry; the
    // ledger doesn't un-judge).
    Gameplay::Result<bool> Resolve(std::uint64_t seq,
                                   IntelVerdict verdict);

    // --- The feeds ---
    // 0..100: share of RESOLVED claims proven false. No resolved
    // claims -> 0 (unjudged intel isn't discredited, just
    // uncertain). The single distortion signal both consumers
    // read.
    int DistortionFor(std::string_view subject) const;

    // 6.3 seam — false verdicts on rival-keyed kinds map to
    // ClaimKind values RenderDossier annotates.
    std::vector<ClaimKind> ProvenWrongFor(
        std::string_view subject) const;

    // Unresolved claims for a subject — briefing-time intel that
    // still carries the Unresolved flag into the chapter.
    std::vector<const IntelEntry*> PendingFor(
        std::string_view subject) const;

    Gameplay::JsonValue ToJson() const;
    static Gameplay::Result<IntelLedger> FromJson(
        const Gameplay::JsonValue& doc);

private:
    std::vector<IntelEntry> entries_;
};

} // namespace Potato::Campaign
