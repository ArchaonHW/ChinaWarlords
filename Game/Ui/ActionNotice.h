#pragma once

#include "Gameplay/Result.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Potato::Game {

// Short-lived feedback for shell actions that cross an I/O boundary.
// Kept as a small value type so result-to-message behavior is headless-testable.
struct ActionNotice {
    bool visible = false;
    bool success = false;
    std::uint32_t remainingMs = 0;
    std::string text;
};

inline constexpr std::uint32_t ACTION_NOTICE_LIFETIME_MS = 3500;

template <typename T>
void ShowActionResult(ActionNotice& notice,
                      const Gameplay::Result<T>& result,
                      std::string_view action) {
    notice.visible = true;
    notice.success = result.ok();
    notice.remainingMs = ACTION_NOTICE_LIFETIME_MS;
    notice.text.assign(action);
    notice.text += result.ok() ? "完成" : "失敗";
    if (!result.ok()) {
        if (!result.error.empty()) {
            notice.text += "（";
            notice.text += result.error;
            notice.text += "）";
        }
        if (!result.reason.empty()) {
            notice.text += "：";
            notice.text += result.reason;
        }
    }
}

inline void AdvanceActionNotice(ActionNotice& notice,
                               std::uint32_t elapsedMs) {
    if (!notice.visible) return;
    if (elapsedMs >= notice.remainingMs) {
        notice.visible = false;
        notice.remainingMs = 0;
        notice.text.clear();
        return;
    }
    notice.remainingMs -= elapsedMs;
}

} // namespace Potato::Game
