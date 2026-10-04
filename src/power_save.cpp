#include "power_save.hpp"

#include <algorithm>

namespace pico_clock {

bool PowerSaveController::valid_test_duration(unsigned seconds) {
    return seconds == 0 || seconds == 5 || seconds == 10 || seconds == 15;
}

bool PowerSaveController::start_test(unsigned seconds, std::uint32_t now_ms) {
    if (!valid_test_duration(seconds)) return false;
    test_active_ = true;
    test_duration_seconds_ = static_cast<std::uint8_t>(seconds);
    test_deadline_ms_ = now_ms + seconds * 1000u;
    active_ = true;
    return true;
}

void PowerSaveController::update(bool normal_display_enabled,
                                 bool button_pressed,
                                 std::uint32_t now_ms) {
    if (button_pressed) test_active_ = false;
    if (test_active_ && test_duration_seconds_ != 0 &&
        static_cast<std::int32_t>(now_ms - test_deadline_ms_) >= 0) {
        test_active_ = false;
    }
    active_ = test_active_ || !normal_display_enabled;
}

bool PowerSaveController::display_enabled(bool normal_display_enabled) const {
    return normal_display_enabled && !test_active_;
}

PowerSaveStatus PowerSaveController::status(std::uint32_t now_ms) const {
    PowerSaveStatus result{};
    result.active = active_;
    result.test_active = test_active_;
    result.test_duration_seconds = test_duration_seconds_;
    if (test_active_ && test_duration_seconds_ != 0) {
        const auto remaining_ms = static_cast<std::int32_t>(
            test_deadline_ms_ - now_ms);
        if (remaining_ms > 0) {
            result.test_seconds_remaining = static_cast<std::uint8_t>(
                std::min<std::int32_t>(255, (remaining_ms + 999) / 1000));
        }
    }
    return result;
}

}  // namespace pico_clock
