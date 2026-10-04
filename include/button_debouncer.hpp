#pragma once

#include <cstdint>

namespace pico_clock {

enum class ButtonEvent {
    None,
    Pressed,
    Released,
};

class ButtonDebouncer {
public:
    ButtonEvent update(bool pressed, std::uint32_t now_ms);
    bool pressed() const { return stable_pressed_; }

private:
    static constexpr std::uint32_t kDebounceMs = 40;

    bool raw_pressed_{false};
    bool stable_pressed_{false};
    std::uint32_t raw_changed_at_ms_{0};
};

}  // namespace pico_clock
