#pragma once

#include "Campaign/Narrative/Bencao.h"
#include "Campaign/Narrative/BencaoCodex.h"
#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Campaign {

// Story 10.3 — the 補鈔 page and its Scribe 批註. When the unlock
// engine (10.2) queues a page, this story's delivery pass drains
// it into the book AND binds a marginal note composed from the
// entry's own unlock manifest — reference becomes memory.
//
// Wire shape (sibling doc, same seam as potato.bencao_state/1):
//   {"schema":"potato.buchao/1",
//    "pages":[{"entry":"sanqi","note":"…","suspect":false}]}
// `pages` is append-ordered (delivery order); each entry appears at
// most once — a page can only leave the pending queue once.
//
// `suspect` is the minimal authenticity plumbing shipped scoped to
// the codex (Story 6.6's ledger-wide marginalia was still backlog
// when this landed — same judgment-overlay semantics as
// `LedgerEntry::suspect`: post-hoc flag, persisted, never hash-
// covered).
struct BuchaoPage {
    std::string entry;    // bencao entry id — the page it annotates
    std::string note;     // 批註 text — composed at delivery
    bool suspect = false; // judgment overlay
};

class BuchaoStore {
public:
    static constexpr std::string_view SCHEMA = "potato.buchao/1";
    // A store can never hold more pages than a library can host.
    static constexpr std::size_t MAX_PAGES = BencaoLibrary::MAX_ENTRIES;
    static constexpr std::size_t MAX_ENTRY_LEN =
        BencaoLibrary::MAX_ID_LEN;
    static constexpr std::size_t MAX_NOTE_LEN = 512;

    const std::vector<BuchaoPage>& Pages() const { return pages_; }
    const BuchaoPage* Find(std::string_view entryId) const;
    // Judgment overlay: set/clear the suspect flag on the page for
    // `entryId`. Returns false when no such page exists.
    bool SetSuspect(std::string_view entryId, bool suspect = true);

    Gameplay::Result<Gameplay::JsonValue> ToJson() const;
    // Rejects bad schema/shape, non-string ids, over-cap pages.
    // Duplicate entries dedupe (first wins); pages naming entries
    // the caller has not delivered are tolerated — judgment
    // metadata, same surface as ledger suspect flags.
    static Gameplay::Result<BuchaoStore> FromJson(
        const Gameplay::JsonValue& doc);

private:
    friend Gameplay::Result<std::vector<BuchaoPage>>
    DeliverBuchao(const BencaoLibrary& lib, BencaoCodex& codex,
                  BuchaoStore& store, std::size_t maxPages,
                  std::int64_t chapterIndex);

    std::vector<BuchaoPage> pages_;
};

// The delivery pass — the 10.3 seam over 10.2's TakePending drain:
// pops up to `maxPages` pending ids (the drain itself writes them
// into the book), composes a 批註 per entry via clause pools keyed
// on `unlockKind`, and appends the pages to `store`. `chapterIndex`
// salts the deterministic clause pick.
//
// Every drained id MUST resolve in `lib` — a pending id with no
// entry is corrupt state and fails `field`, never a skipped row.
// `maxPages == 0` delivers nothing. Idempotent: an entry already
// delivered can't leave the queue again.
Gameplay::Result<std::vector<BuchaoPage>>
DeliverBuchao(const BencaoLibrary& lib, BencaoCodex& codex,
              BuchaoStore& store, std::size_t maxPages,
              std::int64_t chapterIndex);

// 批註 composition — deterministic 史官體 clause pools, one per
// UnlockKind, keyed on the pending page's recorded `kind`
// (falling back to the entry's manifest kind if the persisted
// spelling is unparseable). Pick: FNV-1a(entry.id ‖ page.detail ‖
// chapterIndex LE bytes) % pool size — the same unlock always
// composes the same note (replay-stable; no PRNG, no clock).
// Templates substitute only entry `name` and the provenance
// `detail` (the tag posted / action logged / flag found /
// corruption level / closing chapter) — they may gesture at
// `indications` but never assert efficacy beyond it.
std::string ComposeMarginalia(const BencaoEntry& entry,
                              const PendingPage& page,
                              std::int64_t chapterIndex);

} // namespace Potato::Campaign
