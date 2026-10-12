#include "Campaign/Narrative/BencaoCite.h"

#include "Campaign/Myth/MythLog.h"
#include "Campaign/Narrative/Bencao.h"
#include "Campaign/Narrative/BencaoCodex.h"

#include "Gameplay/Doctrine/Doctrine.h" // SimEvent

namespace Potato::Campaign {

namespace {

using Gameplay::SimEvent;

// Event-kind -> 部類. Derived from the category bindings in
// Bencao.h; kinds that carry no materia association return
// hit=false. 山草/隰草 stay silent — the field doesn't carry
// terrain flags and a colophon must not guess; their unlocks
// ride the terrain path (10.2). 人部 is 10.6's late-beat
// content, likewise uncited here.
bool CiteCategory(SimEvent::Kind k, BencaoCategory& out) {
    switch (k) {
    case SimEvent::Kind::ConvoyArrived:
    case SimEvent::Kind::ConvoyRaided:
        out = BencaoCategory::Manshui; // 河津漕運
        return true;
    case SimEvent::Kind::VillageOccupied:
    case SimEvent::Kind::VillageBurned:
        out = BencaoCategory::Gucai; // 村落糧秣
        return true;
    case SimEvent::Kind::ShrineCaptured:
        out = BencaoCategory::Jinshi; // 祠宇金石
        return true;
    case SimEvent::Kind::InfiltrationChanged:
    case SimEvent::Kind::MythActionInvoked:
    case SimEvent::Kind::MythInvasion:
        out = BencaoCategory::Chongshou; // 靈異
        return true;
    case SimEvent::Kind::SquadExecuted:
        out = BencaoCategory::Ducao; // 暴行墮落
        return true;
    default:
        return false;
    }
}

// 史官體 lead-in per cited category — the report keeps its
// register; the materia's name is the only variable.
const char* ColophonLeadIn(BencaoCategory c) {
    switch (c) {
    case BencaoCategory::Manshui:   return "是役近河津";
    case BencaoCategory::Gucai:     return "是役及村落";
    case BencaoCategory::Jinshi:    return "是役動祠宇";
    case BencaoCategory::Chongshou: return "是役見靈異";
    case BencaoCategory::Ducao:     return "是役有暴行";
    default:                        return "是役所及";
    }
}

} // namespace

std::string RenderBencaoColophon(
    std::span<const SimEvent> events,
    const BencaoCodex& codex, const BencaoLibrary& lib) {
    bool present[kBencaoCategoryCount] = {};
    for (const SimEvent& e : events) {
        BencaoCategory c;
        if (CiteCategory(e.kind, c)) {
            present[static_cast<std::size_t>(c)] = true;
        }
    }
    std::string out;
    for (std::size_t c = 0; c < kBencaoCategoryCount; ++c) {
        if (!present[c]) continue;
        const BencaoCategory cat =
            static_cast<BencaoCategory>(c);
        // First unlocked entry of the category — canonical
        // Entries() order makes the choice stable.
        for (const BencaoEntry& en : lib.Entries()) {
            if (en.category != cat ||
                !codex.IsUnlocked(en.id)) {
                continue;
            }
            out += "書吏按：";
            out += ColophonLeadIn(cat);
            out += "，【";
            out += en.name;
            out += "】之屬已錄冊中。\n";
            break;
        }
    }
    return out;
}

std::string RenderBencaoHearsay(const MythLogEntry& e,
                                const BencaoCodex& codex,
                                const BencaoLibrary& lib) {
    // First canonical-order match wins — deterministic.
    const BencaoEntry* hit = nullptr;
    for (const BencaoEntry& en : lib.Entries()) {
        if (en.unlockKind != UnlockKind::MythState ||
            en.unlockParam != e.action ||
            codex.IsUnlocked(en.id)) {
            continue; // the catalog already spoke — no rumor
        }
        hit = &en;
        break;
    }
    if (hit == nullptr) return {};

    // Folk register, same conventions as MythLog's FolkLine:
    // clerk's coordinates become a folk place-name; the
    // variant picks by seq parity (deterministic, no PRNG).
    const std::string place =
        e.region < 0 ? "某處" : "第" + std::to_string(e.region) + "里";
    const bool alt = (e.seq % 2) == 1;
    std::string out;
    if (hit->aliases.empty()) {
        out = alt ? "或云" + place + "有異草，未之識也。"
                  : "據說" + place + "產靈藥，未之詳也。";
    } else {
        out = alt ? "或云" + place + "有" + hit->aliases[0] +
                        "之屬，未之識也。"
                  : "據說" + place + "出" + hit->aliases[0] +
                        "，山民屢採之。";
    }
    out += '\n';
    return out;
}

} // namespace Potato::Campaign
