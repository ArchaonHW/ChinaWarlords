#pragma once

#include "Campaign/Ledger/Ledger.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Potato::Campaign {

class MarginaliaStore;

// Story 6.8 — Audit Spread (Judgment of the Brush): the
// late-campaign audit event that lays open every forged or
// suspect ledger entry, provenance named per row, and puts the
// Scribe on the stand — an authentic marginalia note
// testifies; a suspect note (or none) stays silent.
//
// HistorianReport counts the taint; this is the INVENTORY the
// count points at. Pure fold over the current head — integer-
// only, deterministic, no PRNG, no I/O; the campaign shell
// decides when "late campaign" is and calls once.
//
// Forged entries fold even where a chronicle's OmissionPolicy
// would hide them — the audit is the truth layer and takes no
// omission policy (HistorianReport's atrocity counter
// precedent).

// One row laid open, seq order.
struct ExposedEntry {
    std::uint64_t seq = 0;
    Provenance provenance = Provenance::Honest;
    bool suspect = false;
    std::string memo;
    std::vector<std::string> tags;
};

// The Scribe's stand on one exposed row. Testimony rides the
// NOTE's own authenticity — the row beneath is tainted by
// definition here, so RenderMarginalia's inherited-taint rule
// would silence every note; the spread asks the note itself.
struct ScribeTestimony {
    std::uint64_t seq = 0;
    bool hasNote = false;     // a note exists on this seq
    bool testifies = false;   // note exists && !note.suspect
    std::string note;         // empty when silent/absent
};

struct AuditSpread {
    std::vector<ExposedEntry> exposed;       // seq order
    std::vector<ScribeTestimony> testimony;  // same order
    std::size_t silent = 0; // rows with no authentic voice

    // Deterministic text render (tooling/tests). 史官體
    // testimony lines; layout/styling is Epic 8 presentation.
    std::string RenderText() const;
};

// Fold: forged ∪ suspect rows in seq order; testimony per row
// from `scribe` (may be null — every row silent).
AuditSpread RenderAuditSpread(const Ledger& l,
                              const MarginaliaStore* scribe);

} // namespace Potato::Campaign
