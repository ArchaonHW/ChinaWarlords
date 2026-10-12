#pragma once

#include <string>
#include <utility>

namespace Potato::Gameplay {

// Boundary result type for all I/O and content loads.
// Sim internals return bool/status enums and never throw; Result<T> is
// used at file/parse/validate boundaries only.
// Call shape per architecture: `if (!json.ok())`, `json.error`, `json.reason`.
template <typename T>
struct Result {
    T value{};
    std::string error;   // error class, e.g. "parse", "schema", "io"
    std::string reason;  // human-readable detail

    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }

    bool ok_ = false;
};

template <typename T>
Result<T> Ok(T v) {
    Result<T> r;
    r.value = std::move(v);
    r.ok_ = true;
    return r;
}

template <typename T>
Result<T> Fail(std::string error, std::string reason) {
    Result<T> r;
    r.error = std::move(error);
    r.reason = std::move(reason);
    return r;
}

} // namespace Potato::Gameplay
