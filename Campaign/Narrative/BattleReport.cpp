#include "Campaign/Narrative/BattleReport.h"

#include "Campaign/Narrative/BencaoCite.h"

#include "Gameplay/Command/Intervention.h"   // InterventionKind
#include "Gameplay/Eval/WinEval.h"           // CloseReason ordinals
#include "Gameplay/Myth/Infiltration.h"      // MythActionKind

#include <cstddef>
#include <initializer_list>
#include <map>
#include <string_view>

namespace Potato::Campaign {

namespace {

using Gameplay::SimEvent;
using K = SimEvent::Kind;

void AppendUInt(std::string& s, std::uint64_t v) {
    char buf[24];
    int n = 0;
    do {
        buf[n++] = static_cast<char>('0' + (v % 10));
        v /= 10;
    } while (v > 0);
    while (n > 0) s += buf[--n];
}

// Clause pools — two variants per narratable deed, picked by count
// parity. The historian's register stays impersonal: deeds happen,
// nobody "does" them.
std::string_view Pick(const std::string_view v[2], std::size_t n) {
    return v[n % 2];
}

} // namespace

std::string_view EllipticalCount(std::size_t n) {
    if (n == 0) return "";
    if (n <= 2) return "一二";
    if (n <= 9) return "數";
    if (n <= 49) return "數十";
    if (n <= 199) return "百餘";
    return "不可勝計";
}

std::string RenderBattleReport(std::span<const SimEvent> events,
                               int historianSide,
                               std::span<const std::uint8_t>
                                   seedLevels,
                               const BencaoCodex* bcodex,
                               const BencaoLibrary* blib) {
    // Fold — every kind lands in exactly one bucket or the
    // confession counter.
    std::size_t occupied[2] = {0, 0};
    std::size_t burned[2] = {0, 0};
    std::size_t arrived = 0, raided = 0;
    std::size_t executed[2] = {0, 0};
    std::size_t shrines[2] = {0, 0};
    std::size_t incursions = 0, pacified = 0;
    std::size_t mythActs[3] = {0, 0, 0}; // by MythActionKind ordinal
    std::size_t invasions = 0;
    std::size_t maneuvers[2] = {0, 0}; // non-myth interventions
    std::size_t omissions = 0;
    // InfiltrationChanged carries the NEW level only — direction is
    // recoverable from the ordered stream: a drop narrates as 禳解,
    // a rise as portent. Carry-in levels seed the baseline so a
    // pacification of haunted ground doesn't misread as incursion.
    std::map<int, int> regionLevel;
    for (std::size_t i = 0; i < seedLevels.size(); ++i) {
        regionLevel[static_cast<int>(i)] = seedLevels[i];
    }
    int winner = -2; // -2 = no ResultDeclared seen
    int closeReason = -1;

    const int us = historianSide;
    const int foe = (historianSide == 0) ? 1 : 0;

    for (const SimEvent& e : events) {
        switch (e.kind) {
            case K::VillageOccupied:
                if (e.side == us) ++occupied[0];
                else if (e.side == foe) ++occupied[1];
                else ++omissions;
                break;
            case K::VillageBurned:
                if (e.side == us) ++burned[0];
                else if (e.side == foe) ++burned[1];
                else ++omissions;
                break;
            case K::ConvoyArrived:
                ++arrived;
                break;
            case K::ConvoyRaided:
                ++raided;
                break;
            case K::SquadExecuted:
                if (e.side == us) ++executed[0];
                else if (e.side == foe) ++executed[1];
                else ++omissions;
                break;
            case K::ShrineCaptured:
                if (e.side == us) ++shrines[0];
                else if (e.side == foe) ++shrines[1];
                else ++omissions;
                break;
            case K::InfiltrationChanged: {
                int& prev = regionLevel[e.param];
                const int prior = prev;
                prev = e.aux;
                if (e.aux > prior) ++incursions;
                else ++pacified;
                break;
            }
            case K::MythActionInvoked:
                if (e.aux >= 0 && e.aux <= 2) ++mythActs[e.aux];
                else ++omissions;
                break;
            case K::MythInvasion:
                // Sustain heartbeats are all confessed — a god's
                // siege needs no stenographer per beat.
                if (e.aux == 2) ++omissions;
                else ++invasions;
                break;
            case K::Intervention:
                // Myth-kind issues are narrated by their
                // MythActionInvoked apply — counting both would
                // double the deed in the telling.
                if (e.aux >= 0 && e.aux <= 6) {
                    if (e.side == us) ++maneuvers[0];
                    else if (e.side == foe) ++maneuvers[1];
                    else ++omissions;
                } else if (e.aux < 0 || e.aux > 9) {
                    ++omissions;
                } // 7-9 myth verbs: silent — apply events narrate
                break;
            case K::ResultDeclared:
                winner = e.param;
                closeReason = e.aux;
                break;
            case K::CardFired:
            case K::BeatChanged:
            default:
                // Below chronicle notice: the doctrine's churn and
                // the drumbeat between verses are confessed, not
                // narrated.
                ++omissions;
                break;
        }
    }

    std::string s = "史官曰：";

    // Opening — the register establishes itself before the deeds.
    s += "兩軍既會，師出以律。\n";

    auto clause = [&](std::size_t n, std::initializer_list<std::string_view> v) {
        if (n == 0) return;
        s += "  ";
        s += Pick(v.begin(), n);
        s += EllipticalCount(n);
        s += "。\n";
    };

    clause(occupied[0], {
                            "王師所過，邑落來歸者",
                            "王師定疆，聚落款附者"});
    clause(occupied[1], {
                            "敵所至，邑落陷者",
                            "敵鋒所及，聚落下者"});
    clause(burned[0], {
                          "王師縱火，聚落焚者",
                          "王師所焚，廬舍燼者"});
    clause(burned[1], {
                          "敵焚聚落",
                          "寇虐所及，廬舍為墟者"});
    clause(arrived, {
                        "糧道既通，饋運至者",
                        "轉輸不絕，軍實繼者"});
    clause(raided, {
                       "輜重見劫者",
                       "糧隊遇襲，委棄於道者"});
    clause(executed[0], {
                            "降卒不赦，誅者",
                            "俘而弗納，斬者"});
    clause(executed[1], {
                            "敵戮降者",
                            "敵忍於降，誅者"});
    clause(maneuvers[0], {
                             "王師臨機之令",
                             "中軍飛檄"});
    clause(maneuvers[1], {
                             "敵之變陣",
                             "敵亦出奇"});
    clause(shrines[0], {
                           "王師奉祀，祠廟歸者",
                           "我軍醮於社，受厘者"});
    clause(shrines[1], {
                           "敵亦祀於社，祠廟歸之者",
                           "敵竊奉明神，祠廟私者"});
    // Portents — side-less, the myth layer narrates as omens.
    clause(incursions, {
                           "是役妖氛所侵",
                           "戰地多祟，陰氣所鍾者"});
    clause(pacified, {
                         "禳解而復靖者",
                         "巫祝有秋，厲氣漸平者"});
    clause(mythActs[0], {
                            "行安撫之祀",
                            "醮於神明，求得平者"});
    clause(mythActs[1], {
                            "降神於卒",
                            "神附於兵，如有異者"});
    clause(mythActs[2], {
                            "發陰兵",
                            "夜召無形之師"});
    clause(invasions, {
                          "神罰降者",
                          "明神震怒，降殃者"});

    // Close — ResultDeclared renders by reason; absent means the
    // record stopped mid-battle (the chronicle confesses a lacuna,
    // not a verdict).
    if (winner >= -1) {
        s += "  ";
        if (winner == us) {
            s += (closeReason == 0)   ? "敵軍盡墨，王師奏捷。"
                 : (closeReason == 1) ? "敵酋請降，王師奏捷。"
                                      : "相持既久，王師奏捷。";
        } else if (winner == foe) {
            s += (closeReason == 0)   ? "王師盡墨，敵奏其捷。"
                 : (closeReason == 1) ? "王師請降，城下之盟成矣。"
                                      : "相持而罷，王師敗績。";
        } else {
            s += "兩軍相持，各罷兵去。";
        }
        s += "\n";
    } else {
        s += "  事未竟，記闕如。\n";
    }

    // The confession — always printed, zero included. The historian
    // admits the silence.
    s += "本報告省略 ";
    AppendUInt(s, omissions);
    s += " 項\n";
    // 書吏按 — the codex colophon rides after the confession:
    // the historian admits what was omitted, then the clerk
    // notes which pages of the book the field touched.
    if (bcodex && blib) {
        s += RenderBencaoColophon(events, *bcodex, *blib);
    }
    return s;
}

} // namespace Potato::Campaign
