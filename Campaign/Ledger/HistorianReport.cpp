#include "Campaign/Ledger/HistorianReport.h"

namespace Potato::Campaign {

namespace {

bool Omitted(const LedgerEntry& e, const OmissionPolicy& p) {
    return (p.omitForged && e.provenance == Provenance::Forged) ||
           (p.omitSuspect && e.suspect);
}

void AppendUInt(std::string& s, std::uint64_t v) {
    char buf[24];
    int n = 0;
    do {
        buf[n++] = static_cast<char>('0' + (v % 10));
        v /= 10;
    } while (v > 0);
    while (n > 0) s += buf[--n];
}

// Strict non-negative integer parse for `chapter:<idx>` payloads —
// same posture as the governance-tag grammar: malformed folds
// nothing.
bool ParseChapterIndex(std::string_view text, std::int64_t& out) {
    if (text.empty() || text.size() > 10) return false;
    std::int64_t v = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    out = v;
    return true;
}

} // namespace

HistorianReport RenderHistorianReport(const Ledger& l,
                                      OmissionPolicy policy) {
    HistorianReport r;
    r.audit.entries = l.Size();
    r.audit.breakReason = l.Verify();
    r.audit.chainOk = (r.audit.breakReason == nullptr);
    r.audit.tip = l.Tip();
    r.audit.seal = l.Seal();
    for (const LedgerEntry& e : l.Entries()) {
        if (e.provenance == Provenance::Forged) ++r.audit.forged;
        if (e.suspect) ++r.audit.suspect;
        std::string_view resolution;
        std::int64_t chapter = -1;
        for (const std::string& t : e.tags) {
            if (t == Ledger::TAG_ATROCITY) {
                ++r.audit.atrocities;
            } else if (t.compare(0, Ledger::TAG_RESOLUTION.size(),
                                 Ledger::TAG_RESOLUTION) == 0 &&
                       resolution.empty()) {
                resolution = std::string_view(t).substr(
                    Ledger::TAG_RESOLUTION.size());
            } else if (t.compare(0, Ledger::TAG_CHAPTER.size(),
                                 Ledger::TAG_CHAPTER) == 0 &&
                       chapter < 0) {
                ParseChapterIndex(
                    std::string_view(t).substr(
                        Ledger::TAG_CHAPTER.size()),
                    chapter);
            }
        }
        // A verdict needs both tags — a lone `resolution:` or
        // `chapter:` is just a label and folds nothing.
        if (!resolution.empty() && chapter >= 0) {
            r.resolutions.push_back(
                ResolutionNote{chapter, std::string(resolution),
                               e.provenance == Provenance::Forged,
                               e.suspect});
        }
        if (Omitted(e, policy)) {
            ++r.omissions;
        } else {
            r.includedSeqs.push_back(e.seq);
        }
    }
    return r;
}

std::string HistorianReport::RenderText() const {
    std::string s;
    s += "entries: ";
    AppendUInt(s, audit.entries);
    s += "\nforged: ";
    AppendUInt(s, audit.forged);
    s += "\nsuspect: ";
    AppendUInt(s, audit.suspect);
    s += "\natrocities: ";
    AppendUInt(s, audit.atrocities);
    s += "\nchain: ";
    s += audit.chainOk ? "ok" : "BROKEN";
    if (audit.breakReason != nullptr) {
        s += " (";
        s += audit.breakReason;
        s += ")";
    }
    s += "\ntip: ";
    AppendUInt(s, audit.tip);
    s += "\nseal: ";
    AppendUInt(s, audit.seal);
    // Chapter verdicts in 史官體 — impersonal epithets, not battle
    // tallies: a governance victory is recorded as rule prevailing,
    // never as a rout (Story 4.4). `kind` spellings are the
    // canonical ResolutionName payloads (pinned by test).
    for (const ResolutionNote& n : resolutions) {
        if (n.chapter < 0) continue; // defensive — fold never emits
        s += "\nresolution chapter ";
        AppendUInt(s, static_cast<std::uint64_t>(n.chapter));
        s += ": ";
        if (n.kind == "governance_victory") {
            s += "governance victory \xE2\x80\x94 \xE4\xBB\xA5\xE6"
                 "\xB2\xBB\xE7\x82\xBA\xE5\x8B\x9D";
        } else if (n.kind == "battle_victory") {
            s += "battle victory \xE2\x80\x94 \xE4\xBB\xA5\xE5\x85"
                 "\xB5\xE5\x8F\x96\xE4\xB9\x8B";
        } else if (n.kind == "defeat") {
            s += "defeat \xE2\x80\x94 \xE6\x95\x97\xE8\x80\x8C\xE6"
                 "\x9C\xAA\xE7\xB5\x95";
        } else {
            // Unrecognized payload: render sanitized — tags are
            // length-checked but may carry control bytes/newlines;
            // verbatim would let a forged tag inject report lines.
            for (char c : n.kind) {
                s += (c >= 0x20 && c < 0x7F) ? c : '?';
            }
        }
        // Doubt is annotated per line, not just in the aggregate
        // audit counters — a forged verdict never reads as honest.
        if (n.forged) s += " (forged)";
        if (n.suspect) s += " (suspect)";
    }
    // 本報告省略 N 項 — epics.md's literal confession marker.
    s += "\n\xE6\x9C\xAC\xE5\xA0\xB1\xE5\x91\x8A\xE7\x9C\x81\xE7\x95\xA5 ";
    AppendUInt(s, omissions);
    s += " \xE9\xA0\x85\n";
    return s;
}

} // namespace Potato::Campaign
