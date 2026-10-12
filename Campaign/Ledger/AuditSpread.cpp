#include "Campaign/Ledger/AuditSpread.h"

#include "Campaign/Narrative/Marginalia.h"

namespace Potato::Campaign {

namespace {

void AppendUInt(std::string& s, std::uint64_t v) {
    char buf[24];
    int n = 0;
    do {
        buf[n++] = static_cast<char>('0' + (v % 10));
        v /= 10;
    } while (v > 0);
    while (n > 0) s += buf[--n];
}

// Memos/notes are content — length-checked at the ledger gate
// but they can still carry control bytes; verbatim would let a
// forged memo inject report lines (HistorianReport precedent).
void AppendSanitized(std::string& s, const std::string& text) {
    for (const char c : text) {
        s += (c >= 0x20 && c < 0x7F) || c < 0 ? c : '?';
    }
}

} // namespace

AuditSpread RenderAuditSpread(const Ledger& l,
                              const MarginaliaStore* scribe) {
    AuditSpread r;
    for (const LedgerEntry& e : l.Entries()) {
        if (e.provenance != Provenance::Forged && !e.suspect) {
            continue;
        }
        r.exposed.push_back(ExposedEntry{
            e.seq, e.provenance, e.suspect, e.memo, e.tags});

        ScribeTestimony t;
        t.seq = e.seq;
        if (scribe != nullptr) {
            if (const MarginaliaNote* n = scribe->Find(e.seq)) {
                t.hasNote = true;
                t.testifies = !n->suspect;
                if (t.testifies) t.note = n->note;
            }
        }
        if (!t.testifies) ++r.silent;
        r.testimony.push_back(std::move(t));
    }
    return r;
}

std::string AuditSpread::RenderText() const {
    std::string s;
    s += "audit: ";
    AppendUInt(s, exposed.size());
    s += " exposed";
    for (std::size_t i = 0; i < exposed.size(); ++i) {
        const ExposedEntry& e = exposed[i];
        const ScribeTestimony& t = testimony[i];
        s += "\nentry ";
        AppendUInt(s, e.seq);
        s += " [";
        if (e.provenance == Provenance::Forged) s += "forged";
        if (e.suspect) {
            s += (e.provenance == Provenance::Forged)
                     ? "+suspect"
                     : "suspect";
        }
        s += "]: ";
        AppendSanitized(s, e.memo);
        s += "\n  ";
        if (t.testifies) {
            // 批者證曰 — the Scribe testifies.
            s += "\xE6\x89\xB9\xE8\x80\x85\xE8\xAD\x89\xE6\x9B"
                 "\xB0\xEF\xBC\x9A";
            AppendSanitized(s, t.note);
        } else if (t.hasNote) {
            // 批者默然 — the note is under question itself.
            s += "\xE6\x89\xB9\xE8\x80\x85\xE9\xBB\x98\xE7\x84"
                 "\xB6";
        } else {
            // （無批） — nothing in the margin.
            s += "\xEF\xBC\x88\xE7\x84\xA1\xE6\x89\xB9\xEF\xBC"
                 "\x89";
        }
    }
    s += "\nsilences: ";
    AppendUInt(s, silent);
    s += "\n";
    return s;
}

} // namespace Potato::Campaign
