#include "Gameplay/Json/Json.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace Potato::Gameplay::Json {

namespace {
// Content files are hand-authored game data, not untrusted input —
// still, cap slurps to bound worst-case DOM memory.
constexpr std::uintmax_t MAX_FILE_BYTES = 64ull * 1024 * 1024;
}

Result<JsonValue> Load(std::string_view path, std::string_view expectedSchema) {
    // u8string path → std::filesystem::path: correct UTF-8 handling on
    // Windows (non-ASCII file names) and passthrough elsewhere.
    const std::filesystem::path fsPath(
        std::u8string(path.begin(), path.end()));

    std::error_code ec;
    const auto size = std::filesystem::file_size(fsPath, ec);
    // A failed stat is an io failure too — and bypassing it would
    // skip the size cap entirely.
    if (ec) {
        return Fail<JsonValue>("io", "cannot stat file: " + std::string(path));
    }
    if (size > MAX_FILE_BYTES) {
        return Fail<JsonValue>("io", "file exceeds 64 MiB cap: " + std::string(path));
    }

    std::ifstream in(fsPath, std::ios::binary);
    if (!in) {
        return Fail<JsonValue>("io", "cannot open file: " + std::string(path));
    }
    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    if (in.bad()) {
        return Fail<JsonValue>("io", "read failure: " + std::string(path));
    }

    if (text.size() >= 2 &&
        ((static_cast<unsigned char>(text[0]) == 0xFF &&
          static_cast<unsigned char>(text[1]) == 0xFE) ||
         (static_cast<unsigned char>(text[0]) == 0xFE &&
          static_cast<unsigned char>(text[1]) == 0xFF))) {
        return Fail<JsonValue>("io", "unsupported encoding (UTF-16); save as UTF-8");
    }
    // UTF-8 BOM — content files may be authored on Windows.
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }

    auto parsed = JsonValue::Parse(text);
    if (!parsed.ok()) return parsed;

    const JsonValue& root = parsed.value;
    if (!root.IsObject()) {
        return Fail<JsonValue>("schema", "root is not an object");
    }
    const std::string* schema = root.FindString("schema");
    if (schema == nullptr) {
        return Fail<JsonValue>("schema", "missing or non-string \"schema\" field");
    }
    if (*schema != expectedSchema) {
        return Fail<JsonValue>(
            "schema", "expected " + std::string(expectedSchema) +
                      ", found " + *schema);
    }
    return parsed;
}

} // namespace Potato::Gameplay::Json
