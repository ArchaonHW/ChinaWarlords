#include "Campaign/Chain/MintOutbox.h"

#include "Gameplay/Json/Json.h"
#include "Gameplay/Json/JsonValue.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <system_error>

namespace Potato::Campaign {

using Gameplay::JsonValue;
using Gameplay::Result;

std::filesystem::path MintOutbox::ClaimPath(const MintClaim& claim) const {
    return dir_ / (ClaimId(claim) + std::string(CLAIM_EXT));
}

Result<std::filesystem::path> MintOutbox::Emit(
    const MintClaim& claim) const {
    Result<JsonValue> doc = claim.ToJson();
    if (!doc.ok()) {
        return Gameplay::Fail<std::filesystem::path>(doc.error,
                                                     doc.reason);
    }
    const std::string text = doc.value.Emit();

    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    if (ec) {
        return Gameplay::Fail<std::filesystem::path>(
            "io", "cannot create outbox dir: " + ec.message());
    }

    const std::filesystem::path dst = ClaimPath(claim);
    const std::filesystem::path tmp =
        dir_ / (ClaimId(claim) + std::string(TMP_EXT));
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return Gameplay::Fail<std::filesystem::path>(
                "io", "cannot open tmp for write");
        }
        out << text;
        out.flush();
        if (!out) {
            return Gameplay::Fail<std::filesystem::path>(
                "io", "tmp write failed");
        }
        out.close(); // explicit: a close-time write error is
                     // observable here, swallowed in the dtor
        if (!out) {
            return Gameplay::Fail<std::filesystem::path>(
                "io", "tmp close failed");
        }
    } // closed before rename (Windows forbids moving open files)

    // Atomic commit — rename replaces the previous claim
    // (MoveFileExW REPLACE_EXISTING on MSVC and MinGW GCC >= 11).
    // On ANY failure we return Fail without touching dst: the old
    // claim stays valid and the complete new one sits in .tmp.
    std::filesystem::rename(tmp, dst, ec);
    if (ec) {
        return Gameplay::Fail<std::filesystem::path>(
            "io", "rename failed: " + ec.message());
    }
    return Gameplay::Ok(dst);
}

Result<MintOutbox::ScanResult> MintOutbox::Scan() const {
    ScanResult res;

    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator it(
             dir_, std::filesystem::directory_options::none, ec);
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
        // Case-folded extension: X.JSON/X.Json are still claims.
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
        return Gameplay::Fail<ScanResult>(
            "io", "outbox dir unreadable: " + ec.message());
    }
    // Deterministic read order: filename sort (id order is applied
    // to committed claims below).
    std::sort(files.begin(), files.end());
    if (files.size() > MAX_CLAIMS) {
        return Gameplay::Fail<ScanResult>(
            "overflow", "outbox claims exceed MAX_CLAIMS");
    }

    for (const std::filesystem::path& f : files) {
        const std::u8string u8 = f.u8string();
        Result<JsonValue> doc = Gameplay::Json::Load(
            std::string_view(
                reinterpret_cast<const char*>(u8.data()),
                u8.size()),
            MintClaim::SCHEMA);
        if (!doc.ok()) {
            res.rejected.push_back({f, doc.error, doc.reason});
            continue;
        }
        Result<MintClaim> claim = MintClaim::FromJson(doc.value);
        if (!claim.ok()) {
            res.rejected.push_back(
                {f, claim.error, claim.reason});
            continue;
        }
        res.claims.push_back(std::move(claim.value));
    }
    std::sort(res.claims.begin(), res.claims.end(),
              [](const MintClaim& a, const MintClaim& b) {
                  return ClaimId(a) < ClaimId(b);
              });
    return Gameplay::Ok(std::move(res));
}

} // namespace Potato::Campaign
