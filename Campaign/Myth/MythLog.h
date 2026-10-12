#pragma once

#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Gameplay { struct SimEvent; }

namespace Potato::Campaign {

// The MythLog (Story 5.4 container; Story 5.6 grows the render +
// folk/rumor register). Myth deeds enter the chronicle BY NAME —
// each entry carries the action's canonical id and its chronicle
// name (CJK surface) so a later voice can retell it without
// re-deriving the catalog.
//
// Region/squad indices are battle-local coordinates — durable for
// the record-of-the-moment (the chronicle remembers WHERE the god
// moved, keyed to the same ground the ledger tags) but callers
// shouldn't treat them as cross-battle identities.
//
// Wire shape:
//   {"schema":"potato.mythlog/1",
//    "entries":[{"seq":0,"action":"pacify_shrine","name":"安撫",
//                "side":0,"region":1,"squad":-1}]}
struct MythLogEntry {
    std::uint64_t seq = 0;
    std::string action;
    std::string name;
    int side = -1;
    int region = -1;
    int squad = -1;
};

class MythLog {
public:
    static constexpr std::string_view SCHEMA = "potato.mythlog/1";
    static constexpr std::size_t MAX_ENTRIES = 4096;
    static constexpr std::size_t MAX_NAME_LEN = 64;

    // Append `action`/`name` for side/region/squad (-1 = none).
    // Returns the entry's seq; fails at capacity — the log is
    // append-only, never partially mutates.
    Gameplay::Result<std::uint64_t> Record(std::string_view action,
                                           std::string_view name,
                                           int side, int region,
                                           int squad);

    const std::vector<MythLogEntry>& Entries() const {
        return entries_;
    }
    std::size_t Size() const { return entries_.size(); }

    Gameplay::Result<Gameplay::JsonValue> ToJson() const;
    static Gameplay::Result<MythLog> FromJson(
        const Gameplay::JsonValue& doc);

private:
    std::vector<MythLogEntry> entries_;
};

// Fold a battle's event stream into the log (Story 5.5): emits a
// chronicle entry for every MythInvasion ("神罰" — the god's own
// move enters the record by name, same as a purchased action).
// MythActionInvoked is deliberately skipped — PerformMythAction
// already logged it at purchase; re-logging here would double-book
// the chronicle. Returns entries appended (append-only contract:
// a failure mid-fold leaves prior entries standing).
Gameplay::Result<std::size_t>
LogMythEvents(MythLog& log,
              std::span<const Gameplay::SimEvent> events);

// Render the log in the folk register (Story 5.6 — narrative's
// "the people talking: rumors, not records"). Every line opens
// with a hearsay marker and none of it is falsifiable by design:
// the folk telling embellishes (ghost hosts are always 數以千計)
// and misattributes (the god's host always marched for "our"
// side) — the register's licensed contradiction. The ledger never
// corrects rumor; rumor never cites the ledger.
//
std::string RenderMythLog(const MythLog& log);

} // namespace Potato::Campaign
