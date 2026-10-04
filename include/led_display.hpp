#pragma once

#include <array>
#include <cstdint>

#include "clock_face.hpp"
#include "pico/time.h"

namespace pico_clock {

class LedDisplay {
public:
    bool init();
    void set_frame(const ClockFace::Frame& frame);
    void set_enabled(bool enabled);
    void set_brightness(std::uint8_t brightness);

private:
    static bool scan_callback(repeating_timer_t* timer);
    void scan_row();

    std::array<ClockFace::Frame, 2> buffers_{};
    volatile unsigned active_buffer_{0};
    volatile bool enabled_{true};
    volatile std::uint8_t brightness_{10};
    unsigned row_{0};
    repeating_timer_t timer_{};
};

}  // namespace pico_clock
