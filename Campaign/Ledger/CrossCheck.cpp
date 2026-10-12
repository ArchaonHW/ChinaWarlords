#include "Campaign/Ledger/CrossCheck.h"

#include "Gameplay/Record/BattleRecorder.h"

#include <algorithm>

namespace Potato::Campaign {

namespace {

constexpr std::string_view kPrefix = "record_root:";

int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

} // namespace

std::string RecordRootTag(std::uint64_t root) {
    std::string s(kPrefix);
    for (int i = 15; i >= 0; --i) {
        s += "0123456789abcdef"[(root >> (i * 4)) & 0xf];
    }
    return s;
}

bool ParseRecordRootTag(std::string_view tag, std::uint64_t& out) {
    if (tag.size() != kPrefix.size() + 16 ||
        tag.compare(0, kPrefix.size(), kPrefix) != 0) {
        return false;
    }
    std::uint64_t v = 0;
    for (std::size_t i = kPrefix.size(); i < tag.size(); ++i) {
        const int d = HexVal(tag[i]);
        if (d < 0) return false;
        v = (v << 4) | static_cast<std::uint64_t>(d);
    }
    out = v;
    return true;
}

CrossCheckResult CrossCheckRecord(
    Ledger& l, const Gameplay::JsonValue& recordDoc) {
    using Gameplay::JsonValue;
    CrossCheckResult r;

    // The record must carry its own integrity root.
    if (!recordDoc.IsObject() || !recordDoc["integrity"].IsObject() ||
        !recordDoc["integrity"]["root"].IsInt()) {
        return r; // BadRecord
    }
    r.declaredRoot = static_cast<std::uint64_t>(
        recordDoc["integrity"]["root"].AsInt());

    // Recompute over the payload (everything except `integrity`) —
    // the same canonical-emit FNV-1a the recorder used.
    JsonValue::Object payload = recordDoc.Members();
    payload.erase("integrity");
    r.recomputedRoot = Gameplay::BattleRecorder::ComputeRoot(
        JsonValue::MakeObject(std::move(payload)));
    const bool selfConsistent =
        (r.recomputedRoot == r.declaredRoot);

    // The ledger itself must verify — an anchor inside a broken
    // chain attests nothing.
    if (l.Verify() != nullptr) {
        r.verdict = CrossVerdict::ChainBroken;
        return r;
    }

    // Collect THIS record's anchors. Relevance: stored root equals
    // the declared or recomputed root — claims matching neither
    // belong to other battles' records and are skipped. Flags mark
    // the disagreement set: an entry is flagged once, and only if
    // it wasn't already suspect.
    for (const LedgerEntry& e : l.Entries()) {
        bool diverging = false;
        bool covered = false;
        for (const std::string& t : e.tags) {
            std::uint64_t stored = 0;
            if (!ParseRecordRootTag(t, stored)) continue;
            if (stored != r.declaredRoot &&
                stored != r.recomputedRoot) {
                continue; // covers a different record
            }
            ++r.anchors;
            if (r.anchors == 1) {
                r.anchorSeq = e.seq;
                r.ledgerRoot = stored;
            }
            covered = true;
            if (!(stored == r.recomputedRoot && selfConsistent)) {
                diverging = true;
            }
        }
        if (covered && e.provenance == Provenance::Forged) {
            ++r.forgedAnchors;
        }
        if (diverging && !e.suspect && l.SetSuspect(e.seq)) {
            ++r.flagged;
        }
    }

    // Verdict. A self-consistent record can only produce Clean or
    // NoAnchor — with dual-relevance, a relevant anchor always
    // agrees with an honest record. Divergence requires a broken
    // seal, so it collapses into RecordInconsistent.
    if (!selfConsistent) {
        r.verdict = CrossVerdict::RecordInconsistent;
    } else if (r.anchors == 0) {
        r.verdict = CrossVerdict::NoAnchor;
    } else {
        r.verdict = CrossVerdict::Clean;
    }
    return r;
}

std::vector<std::uint64_t> OrphanAnchors(
    const Ledger& l,
    const std::vector<std::uint64_t>& presentedRoots) {
    std::vector<std::uint64_t> out;
    for (const LedgerEntry& e : l.Entries()) {
        for (const std::string& t : e.tags) {
            std::uint64_t stored = 0;
            if (!ParseRecordRootTag(t, stored)) continue;
            if (std::find(presentedRoots.begin(), presentedRoots.end(),
                          stored) == presentedRoots.end()) {
                out.push_back(e.seq);
                break; // once per entry is enough
            }
        }
    }
    return out;
}

} // namespace Potato::Campaign
