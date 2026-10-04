#include "button_debouncer.hpp"

namespace pico_clock {

ButtonEvent ButtonDebouncer::update(bool pressed, std::uint32_t now_ms) {
    if (pressed != raw_pressed_) {
        raw_pressed_ = pressed;
        raw_changed_at_ms_ = now_ms;
    }

    if (raw_pressed_ == stable_pressed_ ||
        now_ms - raw_changed_at_ms_ < kDebounceMs) {
        return ButtonEvent::None;
    }

    stable_pressed_ = raw_pressed_;
    return stable_pressed_ ? ButtonEvent::Pressed : ButtonEvent::Released;
}

}  // namespace pico_clock
