#pragma once

#include "Gameplay/Doctrine/Doctrine.h" // SimEvent
#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Gameplay {

class BattleController;
struct SquadTemplate;

// potato.battle_record/1 — the record-is-truth battle journal
// (D-ARCH-6 event sourcing): seed + embedded content docs + an ordered
// planning-input op stream + the sealed SimEvent log. integrity.root =
// FNV-1a over the canonical emit of every field except `integrity` —
// byte-level tamper-evident; semantic tampering that survives a
// recomputed root is caught by the ReplayVerifier's bit-exact diff.
//
// The host mirrors each controller call into a Record* call — the
// recorder is deliberately dumb: it stores what it's told, never reads
// hidden state. Commands need no Record* API: the sealed SimEvent
// stream already carries them (side + payload since Story 1.11).
class BattleRecorder {
public:
    // v2: win evaluation added required `closeReason`/`stalemate`
    // payload fields and the ResultDeclared event kind. Records
    // stamped by v1 tools fail strict field validation — format
    // revisions ride this axis, not the potato.battle_record schema.
    // v3: GovernanceField — five new SimEvent kinds (ordinals 4-8)
    // and the "convoy" planning op; 4.2's Execute intervention +
    // SquadExecuted deed share this version. Older tools reject
    // kind > 3.
    // v4: Epic 5 — InfiltrationChanged (9) + the "mythseed"/"myth"
    // planning ops (5.1), and ShrineCaptured (10) for shrine
    // allegiance flips (5.2). Older tools reject kind > 8.
    // v5: Story 5.4 — MythActionInvoked (11) + InterventionKind
    // aux ordinals 7-9 (MythPacify/MythPossession/MythGhostArmy).
    // Older tools reject kind > 10 / aux > 6.
    // v6: Story 5.5 — MythInvasion (12) for the level-3 deity
    // counterattack cadence. Older tools reject kind > 11.
    static constexpr int TOOL_VERSION = 6;
    static constexpr std::string_view SCHEMA = "potato.battle_record/1";

    // Embedded content makes the record self-contained — the verifier
    // needs no asset pipeline access.
    // Single-use recorder: (re)Binding resets all per-battle state.
    // CONTRACT: mirror ONLY calls the controller accepted (returned
    // true) — a rejected op in the record can never verify.
    void Bind(std::uint64_t seed, const JsonValue& mapDoc,
              const JsonValue& cardsDoc);
    // Optional — but part of the replay contract: fog/plan/eval config
    // ride this doc. A battle run under non-default config without a
    // bound balance doc produces a record that cannot verify.
    void BindBalance(const JsonValue& doc);
    // Records stamped by older tools are flagged `downgrade` on
    // verify — but only verifiable when the checksum algorithm is
    // unchanged (v4 folded MythField in; v5 adds ghosts/possession;
    // v6 adds the invasion cadence tick — so a genuine v5-or-earlier
    // seal can't re-verify; the flag means "older stamp", not
    // "guaranteed replayable").
    void SetToolVersion(int v) { toolVersion_ = v; }

    // Planning inputs — mirror the controller calls, in call order.
    // The template is embedded fully (squad stats are sim state).
    // No-ops after Seal().
    void RecordDeploy(const SquadTemplate& t, std::size_t region,
                      int side);
    void RecordSheet(std::size_t squadIndex,
                     const std::vector<std::string>& cardIds);
    // Argument order mirrors BattleController::DrawArrow(side, squad, path).
    void RecordArrow(int side, std::size_t squadIndex,
                     const std::vector<std::size_t>& path);
    void RecordIntel(int fogSide, int squadIndex, int region,
                     int certainty);
    void RecordCpPool(int side, int cp);
    // Argument order mirrors BattleController::SpawnConvoy(side, path).
    void RecordConvoy(int side, const std::vector<std::size_t>& path);
    // Myth layer (Epic 5.1) — mirror SeedInfiltration /
    // ApplyMythEvent: "mythseed" carries the persisted level in;
    // "myth" journals a table-driven apply.
    void RecordMythSeed(std::size_t region, int level);
    void RecordMyth(std::size_t region, int kind);

    // Captures bc's event stream + checksum + outcome and computes the
    // integrity root. Records are completed battles only: Seal no-ops
    // unless bc is in Aftermath (ToJson then fails as unsealed).
    void Seal(const BattleController& bc);

    // Full record incl. `integrity`. Fails if Bind/Seal never ran.
    Result<JsonValue> ToJson() const;

    // FNV-1a over the canonical emit — verifiers (and tests forging a
    // root-consistent tampered record) recompute it.
    static std::uint64_t ComputeRoot(const JsonValue& payload);

private:
    // Everything except `integrity` — ComputeRoot's input.
    JsonValue PayloadJson() const;

    bool bound_ = false;
    bool sealed_ = false;
    int toolVersion_ = TOOL_VERSION;
    std::uint64_t seed_ = 0;
    JsonValue mapDoc_;     // embedded potato.map/1 doc
    JsonValue cardsDoc_;   // embedded potato.doctrine_cards/1 doc
    JsonValue balanceDoc_; // Null until BindBalance
    std::vector<JsonValue> inputs_; // ordered planning ops
    std::vector<SimEvent> events_;
    std::uint64_t checksum_ = 0;
    int endTick_ = 0;
    int endBeat_ = 0;
    int winner_ = -1;
    int closeReason_ = 0;
    bool forced_ = false;
    bool stalemate_ = false;
};

// SimEvent <-> JSON — shared between recorder emit and verifier parse.
JsonValue EventToJson(const SimEvent& e);
bool EventFromJson(const JsonValue& j, SimEvent& e); // false = malformed

} // namespace Potato::Gameplay
