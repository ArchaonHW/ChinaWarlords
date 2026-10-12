#pragma once

#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <string_view>

namespace Potato::Gameplay::Json {

// Loads a versioned content file and enforces the potato.<name>/<ver>
// schema gate.
//
// - Reads the file via std::ifstream (engine FileSystem module is
//   repo-disabled; boundary I/O only — never call in the tick path).
// - Strips a UTF-8 BOM if present.
// - Root must be a JSON object containing a "schema" string exactly
//   equal to expectedSchema (e.g. "potato.map/1").
// - Any failure returns Fail(...) and mutates nothing — the function
//   has no side effects by construction.
Result<JsonValue> Load(std::string_view path, std::string_view expectedSchema);

} // namespace Potato::Gameplay::Json
