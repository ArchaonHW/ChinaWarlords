#include "Campaign/Narrative/EndingPage.h"

#include "Campaign/Governance/Accumulators.h"
#include "Campaign/Ledger/Ledger.h"

namespace Potato::Campaign {

const char* EndingVoiceName(EndingVoice v) {
    switch (v) {
    case EndingVoice::Believed:  return "believed";
    case EndingVoice::Doubted:   return "doubted";
    case EndingVoice::Forged:    return "forged";
    case EndingVoice::Abandoned: return "abandoned";
    }
    return "believed";
}

bool EndingVoiceFromName(std::string_view name, EndingVoice& out) {
    if (name == "believed") {
        out = EndingVoice::Believed;
    } else if (name == "doubted") {
        out = EndingVoice::Doubted;
    } else if (name == "forged") {
        out = EndingVoice::Forged;
    } else if (name == "abandoned") {
        out = EndingVoice::Abandoned;
    } else {
        return false;
    }
    return true;
}

EndingVoice FoldEndingVoice(const Ledger& l) {
    // The chain can't stand behind its own ending — a book that
    // fails Verify (or was never kept at all) is abandoned by
    // definition, whatever its entries claim.
    if (l.Entries().empty() || l.Verify() != nullptr) {
        return EndingVoice::Abandoned;
    }
    std::size_t forged = 0, suspect = 0;
    for (const auto& e : l.Entries()) {
        if (e.provenance == Provenance::Forged) {
            ++forged;
        } else if (e.suspect) {
            ++suspect;
        }
    }
    if (FoldGovernance(l).corruption >= ABANDON_CORRUPTION) {
        return EndingVoice::Abandoned;
    }
    if (forged > 0) {
        return EndingVoice::Forged;
    }
    if (suspect > 0) {
        return EndingVoice::Doubted;
    }
    return EndingVoice::Believed;
}

std::string RenderEndingPage(const Ledger& l) {
    const EndingVoice v = FoldEndingVoice(l);
    std::string s = "終卷·";
    switch (v) {
    case EndingVoice::Believed:
        s += "信史\n所記皆實，筆筆有據。後世讀此卷，如見其時。\n";
        break;
    case EndingVoice::Doubted: {
        std::size_t n = 0;
        for (const auto& e : l.Entries()) {
            n += e.suspect && e.provenance != Provenance::Forged;
        }
        s += "疑史\n疑筆";
        s += std::to_string(n);
        s += "條未決，真偽之間，讀者自審。\n";
        break;
    }
    case EndingVoice::Forged: {
        std::size_t n = 0;
        for (const auto& e : l.Entries()) {
            n += e.provenance == Provenance::Forged;
        }
        s += "偽史\n偽筆";
        s += std::to_string(n);
        s += "條在冊，與真並陳——後世讀者，不復能分。\n";
        break;
    }
    case EndingVoice::Abandoned:
        s += "絕筆\n卷軸委地，墨盡筆折，無人續寫。\n";
        break;
    }
    return s;
}

} // namespace Potato::Campaign
