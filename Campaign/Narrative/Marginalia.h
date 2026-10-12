#pragma once

#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Potato::Campaign {

class Ledger;
struct LedgerEntry;

// Story 6.6 — the Scribe's 批註 on the ledger itself. Buchao
// (10.3) annotated codex pages; this is the ledger-wide half the
// 10.3 note deferred to: small notes bound to ledger entries by
// `seq`, some authentic, some suspect — a second hand in the
// margin.
//
// Wire shape (sibling doc, same seam as potato.buchao/1):
//   {"schema":"potato.marginalia/1",
//    "notes":[{"seq":3,"note":"…","suspect":false}]}
// `notes` is append-ordered (annotation order); each entry `seq`
// appears at most once — the Scribe annotates an entry once.
//
// `suspect` is the same judgment-overlay convention as
// LedgerEntry::suspect / BuchaoPage::suspect: a post-hoc flag,
// persisted, never hash-covered — the Judgment beat (6.8) flips
// it, the note text is already written.
struct MarginaliaNote {
    std::uint64_t seq = 0; // ledger entry seq — the annotated row
    std::string note;      // 批註 text — composed at annotation
    bool suspect = false;  // judgment overlay
};

class MarginaliaStore {
public:
    static constexpr std::string_view SCHEMA = "potato.marginalia/1";
    // A store can never hold more notes than a ledger can hold
    // entries — same bound, same reason.
    static constexpr std::size_t MAX_NOTES = 4096;
    static constexpr std::size_t MAX_NOTE_LEN = 512;

    const std::vector<MarginaliaNote>& Notes() const {
        return notes_;
    }
    const MarginaliaNote* Find(std::uint64_t seq) const;
    // Judgment overlay: set/clear the suspect flag on the note for
    // `seq`. Returns false when no such note exists.
    bool SetSuspect(std::uint64_t seq, bool suspect = true);

    Gameplay::Result<Gameplay::JsonValue> ToJson() const;
    // Rejects bad schema/shape, non-int seqs, over-cap notes.
    // Duplicate seqs dedupe (first wins); notes naming seqs beyond
    // the ledger's head are tolerated — judgment metadata, same
    // surface as ledger suspect flags.
    static Gameplay::Result<MarginaliaStore> FromJson(
        const Gameplay::JsonValue& doc);

private:
    friend Gameplay::Result<std::size_t>
    AnnotateScribe(const Ledger& l, MarginaliaStore& store,
                   std::size_t maxNotes);

    std::vector<MarginaliaNote> notes_;
};

// The annotation pass — the 6.6 seam mirroring DeliverBuchao:
// scans entries in ledger order, appends a note per ScribeWorthy
// entry not yet annotated, up to `maxNotes` (0 = no-op). The note
// text is composed deterministically from the entry's own content
// — tags choose the clause pool, FNV-1a(memo ‖ seq LE) picks the
// clause; replay-stable, no PRNG, no clock. Idempotent: an
// annotated entry never re-annotates.
Gameplay::Result<std::size_t>
AnnotateScribe(const Ledger& l, MarginaliaStore& store,
               std::size_t maxNotes);

// Which entries earn a margin note — the Scribe only speaks where
// the chronicle turned: atrocity/raid/myth/march tags and the
// resolution:/chapter: markers. Untagged bookkeeping (spend,
// tithe) stays unannotated.
bool ScribeWorthy(const LedgerEntry& e);

// 批註 composition — 史官體 clause pools keyed on the entry's
// salient tag. Deterministic pick: FNV-1a(memo ‖ seq LE bytes) %
// pool size — the same entry always composes the same note.
std::string ComposeScribeNote(const LedgerEntry& e);

// Render pass — one line per note, in annotation order:
//   批〔N〕<note>            — authentic
//   批〔N〕<note>（疑）       — suspect
// A note renders suspect when its own flag is set OR the entry it
// annotates carries ledger `suspect` — the ink is tainted by the
// row beneath it either way.
std::string RenderMarginalia(const MarginaliaStore& store,
                             const Ledger& l);

} // namespace Potato::Campaign
