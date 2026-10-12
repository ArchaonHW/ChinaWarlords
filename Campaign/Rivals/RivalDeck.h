#pragma once

#include "Gameplay/Doctrine/Doctrine.h"
#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Campaign {

class IntelLedger; // Narrative/IntelLedger.h — distortion feed

// RivalDeck learning (Story 3.7): cross-chapter hearsay about
// the player's habitual triggers, driving counter-decks.
//
// The dossier is deliberately thin — aggregates only, no
// per-battle detail. Enemy generals exist as hearsay (傳聞),
// never as omniscient stat sheets (GDD RivalDeck pillar).

// Personality prior — biases doctrine authorship and fog shape
// (wired in Epic E0/C; here it's persisted identity, not
// mechanics).
enum class RivalPrior : std::uint8_t {
    Aggressive = 0,
    Defensive = 1,
    Cunning = 2,
};
const char* RivalPriorName(RivalPrior p);
bool RivalPriorFromName(std::string_view name, RivalPrior& out);

// Wire-format stability: the histogram serializes as an object
// keyed by TriggerKindWire names (not a positional array), and
// this constant is asserted against the enum — adding a
// TriggerKind without updating RivalDeck breaks the build, not
// a save.
constexpr std::size_t kTriggerKindCount = 4;

using TriggerHistogram = std::array<std::int64_t,
                                    kTriggerKindCount>;

// One enemy general's accumulated read on the player.
struct GeneralDossier {
    std::string id;
    RivalPrior prior = RivalPrior::Aggressive;
    std::int64_t chaptersObserved = 0;
    TriggerHistogram triggers{}; // learned usage counts
};

// The learning store — one dossier per named rival, persisted
// as potato.rivals/1. `entries` ordered by first sighting
// (deterministic: insertion order, not sorted — hearsay arrives
// when it arrives).
class RivalBook {
public:
    static constexpr std::string_view SCHEMA =
        "potato.rivals/1";
    static constexpr std::size_t MAX_RIVALS = 64;
    static constexpr std::size_t MAX_ID_LEN = 64;
    static constexpr std::int64_t MAX_COUNT = 1000000;
    // AC's "three chapters": below this the hearsay hasn't
    // formed and PrepareCounterDeck returns an empty deck.
    static constexpr std::int64_t MIN_CHAPTERS_TO_LEARN = 3;

    std::size_t Count() const { return dossiers_.size(); }
    // Pointer valid until the next RecordChapter (push_back
    // may reallocate).
    const GeneralDossier* Find(std::string_view id) const;
    const std::vector<GeneralDossier>& Dossiers() const {
        return dossiers_;
    }

    // Folds one chapter's observed trigger usage into the
    // dossier — creating it on first sighting. `prior` is
    // sticky: a re-observed rival keeps its first-sighting
    // personality (hearsay identity, not a live feed).
    // Negative or oversized counts reject; a rejected call
    // never leaves a partial dossier. Returns the dossier's
    // index in Dossiers().
    Gameplay::Result<int> RecordChapter(
        std::string_view id, RivalPrior prior,
        const TriggerHistogram& usage);

    // The rival's answer for the next chapter. Empty when:
    // rival unknown, observed < MIN_CHAPTERS_TO_LEARN, depth <=
    // 0, or no trigger was ever observed. Otherwise emits up to
    // `depth` counter-card ids — "counter.<trigger>" — ordered
    // by the player's most-used triggers first (ties by
    // ordinal; deterministic). Only ids resolving in `counters`
    // are emitted: counter-cards are content (Epic 9), this
    // emits intent.
    //
    // `intel` (Story 6.4): the rival's proven-wrong rate on its
    // intel about us erodes counter depth — a rival whose
    // briefing was false counters shallower.
    std::vector<std::string> PrepareCounterDeck(
        std::string_view id,
        const Gameplay::DoctrineLibrary& counters,
        int depth, const IntelLedger* intel = nullptr) const;

    Gameplay::Result<Gameplay::JsonValue> ToJson() const;
    static Gameplay::Result<RivalBook> FromJson(
        const Gameplay::JsonValue& doc);

private:
    std::vector<GeneralDossier> dossiers_;
};

// Wire id for a TriggerKind (drives the counter.<id>
// convention and keeps the JSON histogram readable).
const char* TriggerKindWire(Gameplay::TriggerKind t);

} // namespace Potato::Campaign
