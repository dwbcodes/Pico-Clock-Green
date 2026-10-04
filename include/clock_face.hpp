#pragma once

#include <array>
#include <cstdint>

namespace pico_clock {

class ClockFace {
public:
    static constexpr int kWidth = 24;
    static constexpr int kHeight = 8;
    using Frame = std::array<std::uint32_t, kHeight>;

    static Frame render_time(int hour, int minute, int second, int weekday,
                             bool blink_on);
    static Frame render_date(int month, int day, int weekday);
    static Frame render_temperature(float celsius, bool fahrenheit);
    static Frame render_reset_countdown(int remaining_seconds, bool blink_on);
    static Frame render_text(const char* text, std::uint32_t scroll_step);
    static void apply_status_indicators(Frame& frame,
                                        bool automatic_brightness,
                                        bool chime_enabled = false);
    static bool pixel(const Frame& frame, int x, int y);

private:
    static void draw_digit(Frame& frame, int x, int digit);
    static void draw_second_progress(Frame& frame, int second, bool blink_on);
    static void draw_progress(Frame& frame, int completed, bool blink_current);
    static void draw_four_digits(Frame& frame, int value);
    static void draw_weekday(Frame& frame, int weekday);
    static void draw_meridiem(Frame& frame, int hour);
    static void set_pixel(Frame& frame, int x, int y);
    static void clear_pixel(Frame& frame, int x, int y);
};

}  // namespace pico_clock
