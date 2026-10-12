#pragma once

#include <span>
#include <string>

namespace Potato::Gameplay { struct SimEvent; }

namespace Potato::Campaign {

class BencaoLibrary;
class BencaoCodex;
struct MythLogEntry;

// Story 10.5 — the chronicle's other voices notice the book.
// Both helpers are pure folds: deterministic, no PRNG, no I/O,
// never write codex/sim state. Each returns lines to append
// (empty when nothing qualifies), composed by the CALLER —
// RenderMythLog's own signature is untouched, so the citation
// layer is removable by construction.
//
// The direction rule, pinned:
//   史官 colophon — names ONLY already-unlocked entries by
//                   their canonical 正名: the book verifies
//                   what the field saw.
//   市井 hearsay  — names ONLY not-yet-unlocked entries whose
//                   myth_state trigger this log entry just
//                   fired, and names them by folk 釋名
//                   (aliases[0]; 靈藥/異草 when aliasless) —
//                   rumor precedes the catalog, never the
//                   reverse, and a sealed page's 正名 never
//                   leaks. A 補鈔-pending page is still fair
//                   game — the codex isn't verified until the
//                   page lands in `unlocked`.

// Colophon for one battle — at most one citation per 部類
// (first unlocked entry in canonical Entries() order),
// category-ordinal order. Event kinds that carry no materia
// association contribute nothing. (codex first — the unlock
// state is the gate, the library is reference data.)
std::string RenderBencaoColophon(
    std::span<const Gameplay::SimEvent> events,
    const BencaoCodex& codex, const BencaoLibrary& lib);

// Hearsay for ONE myth log entry — at most one line (the
// first canonical match), in the folk register: place-aware,
// seq-parity variant like the log's own FolkLine. Empty
// string when nothing qualifies. Compose per entry after the
// log's own lines.
std::string RenderBencaoHearsay(const MythLogEntry& e,
                                const BencaoCodex& codex,
                                const BencaoLibrary& lib);

} // namespace Potato::Campaign
