#include "Campaign/Narrative/NarrativePack.h"

#include "Gameplay/Json/Json.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <system_error>

namespace Potato::Campaign {

using Gameplay::JsonValue;
using Gameplay::Result;

NarrativeLoadResult
NarrativeLibrary::Load(const std::filesystem::path& dir,
                       NarrativeLibrary& out) {
    NarrativeLoadResult res;

    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator it(
             dir, std::filesystem::directory_options::none, ec);
         !ec && it != std::filesystem::directory_iterator();
         it.increment(ec)) {
        // Per-entry stat uses its OWN error channel (ChapterLibrary
        // precedent — shared `ec` gets cleared by increment()).
        std::error_code ec2;
        const bool isFile = it->is_regular_file(ec2);
        if (ec2) {
            res.rejected.push_back(
                {it->path(), "io",
                 "stat failed: " + ec2.message()});
            continue;
        }
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
        res.reason = "narrative dir unreadable: " + ec.message();
        return res;
    }
    // Deterministic merge order: filename sort.
    std::sort(files.begin(), files.end());
    if (files.size() > MAX_PACKS) {
        res.error = "overflow";
        res.reason = "narrative file count exceeds MAX_PACKS";
        return res;
    }

    // Merged chapters document built incrementally — per-file
    // validation runs first, so only accepted members land here.
    JsonValue::Object merged;
    JsonValue::Object mergedVariants;
    std::set<std::string> packIds;
    std::set<std::string> chapterIds;
    std::set<std::string> docIds; // 6.7 — variant doc namespace
    std::vector<NarrativePackInfo> packs;
    for (const std::filesystem::path& f : files) {
        const std::u8string u8 = f.u8string();
        Result<JsonValue> doc = Gameplay::Json::Load(
            std::string_view(
                reinterpret_cast<const char*>(u8.data()),
                u8.size()),
            SCHEMA);
        if (!doc.ok()) {
            res.rejected.push_back({f, doc.error, doc.reason});
            continue;
        }
        Gameplay::Result<ChapterConventions> conv =
            ChapterConventions::FromJson(doc.value);
        if (!conv.ok()) {
            res.rejected.push_back({f, conv.error, conv.reason});
            continue;
        }

        // Pack id: optional "pack" field, else filename stem.
        std::string packId;
        const std::string* p = doc.value.FindString("pack");
        if (p != nullptr) {
            packId = *p;
        } else {
            const std::u8string stem = f.stem().u8string();
            packId = std::string(
                reinterpret_cast<const char*>(stem.data()),
                stem.size());
        }
        if (packId.empty() ||
            packId.size() > MAX_PACK_ID_LEN) {
            res.rejected.push_back({f, "field", "bad pack id"});
            continue;
        }
        if (packIds.count(packId)) {
            res.rejected.push_back(
                {f, "duplicate", "pack id already registered"});
            continue;
        }
        // Chapter ids are one namespace across packs.
        const JsonValue& chapters = doc.value["chapters"];
        bool dup = false;
        for (const auto& [id, _] : chapters.Members()) {
            if (chapterIds.count(id)) {
                res.rejected.push_back(
                    {f, "duplicate",
                     "chapter id already registered: " + id});
                dup = true;
                break;
            }
        }
        if (dup) continue;
        if (chapterIds.size() + chapters.Size() >
            ChapterConventions::MAX_CHAPTERS) {
            res.rejected.push_back(
                {f, "field", "merged chapter space full"});
            continue;
        }
        // 6.7 — variant doc names are also one namespace; a
        // later pack repeating a doc name rejects the file.
        const JsonValue& packVariants = doc.value["variants"];
        if (packVariants.IsObject()) {
            for (const auto& [dname, _] :
                 packVariants.Members()) {
                if (docIds.count(dname)) {
                    res.rejected.push_back(
                        {f, "duplicate",
                         "variant doc already registered: " +
                             dname});
                    dup = true;
                    break;
                }
            }
            if (dup) continue;
        }
        // Commit: no partial merge — all-or-nothing per file.
        for (const auto& [id, cv] : chapters.Members()) {
            merged[id] = cv;
            chapterIds.insert(id);
        }
        if (packVariants.IsObject()) {
            for (const auto& [dname, dv] :
                 packVariants.Members()) {
                mergedVariants[dname] = dv;
                docIds.insert(dname);
            }
        }
        packIds.insert(packId);
        packs.push_back({packId, chapters.Size()});
    }

    JsonValue::Object root;
    root["schema"] = JsonValue::String(std::string(SCHEMA));
    root["chapters"] = JsonValue::MakeObject(std::move(merged));
    if (!mergedVariants.empty()) {
        root["variants"] =
            JsonValue::MakeObject(std::move(mergedVariants));
    }
    // Re-parse the merged doc so `merged_` carries the exact same
    // shape the single-file path produces — one parser, one truth.
    Gameplay::Result<ChapterConventions> all =
        ChapterConventions::FromJson(
        JsonValue::MakeObject(std::move(root)));
    if (!all.ok()) {
        res.error = all.error;
        res.reason = all.reason;
        return res;
    }
    out.merged_ = std::move(all.value);
    out.packs_ = std::move(packs);
    res.ok = true;
    return res;
}

} // namespace Potato::Campaign
