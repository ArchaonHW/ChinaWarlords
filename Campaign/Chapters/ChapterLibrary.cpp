#include "Campaign/Chapters/ChapterLibrary.h"

#include "Campaign/State/CampaignState.h"
#include "Gameplay/Json/Json.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace Potato::Campaign {

// The library bound and the save format's bound must agree —
// drift breaks the 3.4 chapter-space contract.
static_assert(ChapterLibrary::MAX_CHAPTERS ==
                  CampaignState::MAX_CHAPTERS,
              "chapter space bound drifted from CampaignState");

using Gameplay::JsonValue;
using Gameplay::Result;

namespace {

constexpr std::size_t kMaxBindIds = 32;

const char* ParseIdArray(const JsonValue& arr,
                         std::vector<std::string>& out) {
    if (arr.Size() > kMaxBindIds) return "bind id array too long";
    for (const JsonValue& item : arr.Items()) {
        if (!item.IsString()) return "bind id not a string";
        const std::string& id = item.AsString();
        if (id.empty() || id.size() > ChapterLibrary::MAX_ID_LEN) {
            return "bind id out of range";
        }
        out.push_back(id);
    }
    return nullptr;
}

const char* ParseControl(const std::string& s, WorldControl& out) {
    if (s == "neutral") out = WorldControl::Neutral;
    else if (s == "player") out = WorldControl::Player;
    else if (s == "rival") out = WorldControl::Rival;
    else return "bind control unknown";
    return nullptr;
}

const char* ValidateBind(const JsonValue& bind, ChapterBind& out) {
    // New vocabulary inside `bind` is strict — typos reject.
    for (const auto& kv : bind.Members()) {
        const std::string& key = kv.first;
        if (key != "node" && key != "control" && key != "resolved" &&
            key != "requires" && key != "ledger" &&
            key != "mandatory" && key != "beat") {
            return "bind unknown key";
        }
    }
    if (const std::string* node = bind.FindString("node")) {
        if (node->empty() || node->size() > ChapterLibrary::MAX_ID_LEN) {
            return "bind node out of range";
        }
        out.node = *node;
    } else if (bind.Has("node")) {
        return "bind node mistyped";
    }
    if (bind.Has("control")) {
        const std::string* s = bind.FindString("control");
        if (!s) return "bind control mistyped";
        if (const char* why = ParseControl(*s, out.control)) {
            return why;
        }
        out.hasControl = true;
    }
    if (bind.Has("resolved")) {
        if (!bind["resolved"].IsArray()) return "bind resolved mistyped";
        if (const char* why =
                ParseIdArray(bind["resolved"], out.resolved)) {
            return why;
        }
    }
    if (bind.Has("requires")) {
        if (!bind["requires"].IsArray()) return "bind requires mistyped";
        if (const char* why =
                ParseIdArray(bind["requires"], out.prereqs)) {
            return why;
        }
    }
    if (bind.Has("ledger")) {
        const JsonValue& led = bind["ledger"];
        if (!led.IsObject()) return "bind ledger mistyped";
        for (const auto& kv : led.Members()) {
            if (kv.first != "axis" && kv.first != "at_least") {
                return "bind ledger unknown key";
            }
        }
        const std::string* ax = led.FindString("axis");
        if (!ax || !led["at_least"].IsInt()) {
            return "bind ledger missing field";
        }
        if (*ax == "popular_support") {
            out.axis = LedgerAxis::PopularSupport;
        } else if (*ax == "order") {
            out.axis = LedgerAxis::Order;
        } else if (*ax == "corruption") {
            out.axis = LedgerAxis::Corruption;
        } else {
            return "bind ledger axis unknown";
        }
        const std::int64_t at = led["at_least"].AsInt();
        if (at < 1) return "bind ledger at_least out of range";
        out.atLeast = at;
        out.hasLedger = true;
    }
    if (bind.Has("mandatory")) {
        if (!bind["mandatory"].IsBool()) return "bind mandatory mistyped";
        out.mandatory = bind["mandatory"].AsBool();
    }
    if (bind.Has("beat")) {
        const std::string* b = bind.FindString("beat");
        if (!b) return "bind beat mistyped";
        if (*b == "commission") {
            out.beat = ChapterBeat::Commission;
        } else if (*b == "pivot") {
            out.beat = ChapterBeat::Pivot;
        } else if (*b == "final") {
            out.beat = ChapterBeat::Final;
        } else {
            return "bind beat unknown";
        }
    }
    // A held-region predicate with no region is meaningless.
    if (out.hasControl && out.node.empty()) {
        return "bind control requires node";
    }
    return nullptr;
}

// One file -> ChapterDef or a reason string. Pure validation:
// unknown members are ignored (forward-compat).
const char* ValidateChapter(const JsonValue& doc, ChapterDef& out) {
    if (!doc.IsObject()) return "root is not an object";
    const std::string* id = doc.FindString("id");
    const std::string* title = doc.FindString("title");
    const std::string* map = doc.FindString("map");
    const std::string* briefing = doc.FindString("briefing");
    if (!id || !title || !map || !doc["index"].IsInt() ||
        !doc["combat"].IsBool()) {
        return "missing/mistyped field";
    }
    if (id->empty() || id->size() > ChapterLibrary::MAX_ID_LEN) {
        return "id out of range";
    }
    if (title->empty() ||
        title->size() > ChapterLibrary::MAX_TITLE_LEN) {
        return "title out of range";
    }
    if (map->empty() || map->size() > ChapterLibrary::MAX_REF_LEN) {
        return "map ref out of range";
    }
    if (doc.Has("briefing") && !doc["briefing"].IsString()) {
        return "missing/mistyped field";
    }
    if (briefing &&
        briefing->size() > ChapterLibrary::MAX_BRIEFING_LEN) {
        return "briefing too long";
    }
    const std::int64_t idx = doc["index"].AsInt();
    if (idx < 0 ||
        static_cast<std::uint64_t>(idx) >=
            ChapterLibrary::MAX_CHAPTERS) {
        return "index out of range";
    }
    out.id = *id;
    out.index = idx;
    out.title = *title;
    out.map = *map;
    out.combat = doc["combat"].AsBool();
    out.briefing = briefing ? *briefing : "";
    if (doc.Has("bind")) {
        if (!doc["bind"].IsObject()) return "bind mistyped";
        out.bound = true;
        return ValidateBind(doc["bind"], out.bind);
    }
    return nullptr;
}

} // namespace

ChapterLoadResult ChapterLibrary::Load(
    const std::filesystem::path& dir, ChapterLibrary& out) {
    ChapterLoadResult res;

    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator it(
             dir, std::filesystem::directory_options::none, ec);
         !ec && it != std::filesystem::directory_iterator();
         it.increment(ec)) {
        // Per-entry stat uses its OWN error channel — sharing `ec`
        // would let a stat failure be cleared by the next
        // increment() and silently drop the file.
        std::error_code ec2;
        const bool isFile = it->is_regular_file(ec2);
        if (ec2) {
            res.rejected.push_back(
                {it->path(), "io", "stat failed: " + ec2.message()});
            continue;
        }
        // Case-folded extension: X.JSON/X.Json are still content.
        std::string ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
        if (isFile && ext == ".json") {
            files.push_back(it->path());
        }
    }
    if (ec) {
        res.error = "io";
        res.reason = "chapter dir unreadable: " + ec.message();
        return res;
    }
    // Deterministic registration order: filename sort.
    std::sort(files.begin(), files.end());

    std::vector<ChapterDef> chapters;
    std::vector<std::filesystem::path> chapterPaths;
    std::set<std::string> ids;
    std::set<std::int64_t> indexes;
    for (const std::filesystem::path& f : files) {
        const std::u8string u8 = f.u8string();
        Result<JsonValue> doc = Gameplay::Json::Load(
            std::string_view(
                reinterpret_cast<const char*>(u8.data()),
                u8.size()),
            SCHEMA);
        if (!doc.ok()) {
            res.rejected.push_back(
                {f, doc.error, doc.reason});
            continue;
        }
        ChapterDef def;
        if (const char* why =
                ValidateChapter(doc.value, def)) {
            res.rejected.push_back({f, "field", why});
            continue;
        }
        // Both checks BEFORE either commit — a file rejected for a
        // dup index must not consume its id for later files.
        if (ids.count(def.id)) {
            res.rejected.push_back(
                {f, "duplicate", "chapter id already registered"});
            continue;
        }
        if (indexes.count(def.index)) {
            res.rejected.push_back({f, "duplicate",
                                    "chapter index already used"});
            continue;
        }
        ids.insert(def.id);
        indexes.insert(def.index);
        chapterPaths.push_back(f);
        chapters.push_back(std::move(def));
    }

    // Cross-file pass: bind.requires edges must land on chapters
    // that survived validation. Fixpoint prune — dropping a
    // candidate can orphan another's edge; loop until stable.
    if (!chapters.empty()) {
        bool changed = true;
        while (changed) {
            changed = false;
            for (std::size_t i = 0; i < chapters.size(); ++i) {
                const ChapterBind& b = chapters[i].bind;
                if (!chapters[i].bound) continue;
                const std::string* missing = nullptr;
                for (const std::string& req : b.prereqs) {
                    if (ids.count(req) == 0) {
                        missing = &req;
                        break;
                    }
                }
                if (!missing) continue;
                res.rejected.push_back(
                    {chapterPaths[i],
                     "field",
                     "bind requires unknown chapter '" + *missing +
                         "'"});
                ids.erase(chapters[i].id);
                indexes.erase(chapters[i].index);
                chapters.erase(chapters.begin() +
                               static_cast<std::ptrdiff_t>(i));
                chapterPaths.erase(
                    chapterPaths.begin() +
                    static_cast<std::ptrdiff_t>(i));
                changed = true;
                break; // restart the scan after mutation
            }
        }
    }

    // Library order = campaign order.
    std::sort(chapters.begin(), chapters.end(),
              [](const ChapterDef& a, const ChapterDef& b) {
                  return a.index < b.index;
              });
    out.chapters_ = std::move(chapters);
    res.ok = true;
    return res;
}

const ChapterDef* ChapterLibrary::Find(std::string_view id) const {
    for (const ChapterDef& c : chapters_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

const ChapterDef* ChapterLibrary::AtIndex(std::int64_t index) const {
    for (const ChapterDef& c : chapters_) {
        if (c.index == index) return &c;
    }
    return nullptr;
}

} // namespace Potato::Campaign
