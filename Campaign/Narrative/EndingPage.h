#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Potato::Campaign {

class Ledger;

// Story 6.9 — the ending folio, four voices. When the campaign's
// last chapter closes, the ledger folds one final time and the
// book tells the player what they wrote. There is no selection
// surface: the verdict is a pure function of ledger state
// (integrity chain, provenance, suspicion, the corruption
// ratchet), so replays and tampered saves get exactly the ending
// their entries earned.
//
//   believed   信史 — clean chain, nothing forged, nothing
//               suspect: the chronicle can be read straight
//   doubted    疑史 — no forged rows, but suspect flags stand:
//               the record survives under question
//   forged     偽史 — forged entries are in the book: a
//               counterfeit passage sits inside the chronicle
//   abandoned  絕筆 — the chain itself fails to verify, the
//               book was never kept (empty), or the corruption
//               ratchet drowned the hand that held the brush
//
// Precedence is abandonment → forgery → doubt → belief: the
// harsher verdict always wins — a book with both a forged page
// and a suspect note closes as forged.
enum class EndingVoice : std::uint8_t {
    Believed = 0,
    Doubted,
    Forged,
    Abandoned,
};
// Stable ASCII wire id ("believed", …) — round-trips with
// *FromName. CJK folio titles live in the renderer.
const char* EndingVoiceName(EndingVoice v);
bool EndingVoiceFromName(std::string_view name, EndingVoice& out);

// The fold itself — one read of the final state, no side effects.
// `abandoned` also fires when FoldGovernance's corruption peak
// reaches ABANDON_CORRUPTION: a one-way debt that high means the
// chronicle was being written by cruelty, and the hand stopped.
EndingVoice FoldEndingVoice(const Ledger& l);
// Ratchet level at which the book counts as abandoned — a
// story-level threshold, deliberately a named constant rather
// than a magic literal in the fold.
constexpr std::int64_t ABANDON_CORRUPTION = 100;

// The rendered folio (史官體, deterministic, integer-only). Four
// stanzas, one per voice; the forged/doubted folios confess their
// counts — 偽筆 N 條、疑筆 N 條 — same confession discipline as
// HistorianReport and the audit spread. An abandoned book still
// gets a page: silence is also a verdict.
std::string RenderEndingPage(const Ledger& l);

} // namespace Potato::Campaign
