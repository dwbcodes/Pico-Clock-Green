#include "clock_face.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

namespace pico_clock {
namespace {

// Waveshare's reference numeric font is 4x7. Its bits are ordered from the
// physical left edge at bit 0 because the SM16106 chain receives bytes LSB-first.
constexpr std::array<std::array<std::uint8_t, 7>, 10> kDigits{{
    {{0b0110, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110}},
    {{0b0100, 0b0110, 0b0100, 0b0100, 0b0100, 0b0100, 0b1110}},
    {{0b0110, 0b1001, 0b1000, 0b0100, 0b0010, 0b0001, 0b1111}},
    {{0b0110, 0b1001, 0b1000, 0b0110, 0b1000, 0b1001, 0b0110}},
    {{0b1000, 0b1100, 0b1010, 0b1001, 0b1111, 0b1000, 0b1000}},
    {{0b1111, 0b0001, 0b0111, 0b1000, 0b1000, 0b1001, 0b0110}},
    {{0b0100, 0b0010, 0b0001, 0b0111, 0b1001, 0b1001, 0b0110}},
    {{0b1111, 0b1001, 0b0100, 0b0100, 0b0100, 0b0100, 0b0100}},
    {{0b0110, 0b1001, 0b1001, 0b0110, 0b1001, 0b1001, 0b0110}},
    {{0b0110, 0b1001, 0b1001, 0b1110, 0b1000, 0b0100, 0b0010}},
}};

using Glyph = std::array<std::uint8_t, 7>;
using SmallGlyph = std::array<std::uint8_t, 5>;

constexpr std::array<SmallGlyph, 10> kSmallDigits{{
    {{0b111, 0b101, 0b101, 0b101, 0b111}},
    {{0b010, 0b011, 0b010, 0b010, 0b111}},
    {{0b111, 0b100, 0b111, 0b001, 0b111}},
    {{0b111, 0b100, 0b111, 0b100, 0b111}},
    {{0b101, 0b101, 0b111, 0b100, 0b100}},
    {{0b111, 0b001, 0b111, 0b100, 0b111}},
    {{0b111, 0b001, 0b111, 0b101, 0b111}},
    {{0b111, 0b100, 0b100, 0b100, 0b100}},
    {{0b111, 0b101, 0b111, 0b101, 0b111}},
    {{0b111, 0b101, 0b111, 0b100, 0b111}},
}};

const Glyph& text_glyph(char character) {
    static constexpr Glyph blank{{0, 0, 0, 0, 0, 0, 0}};
    static constexpr Glyph unknown{{0b0110, 0b1001, 0b1000, 0b0100, 0b0100, 0, 0b0100}};
    static constexpr std::array<Glyph, 26> letters{{
        {{0b0110, 0b1001, 0b1001, 0b1111, 0b1001, 0b1001, 0b1001}}, // A
        {{0b0111, 0b1001, 0b1001, 0b0111, 0b1001, 0b1001, 0b0111}}, // B
        {{0b1110, 0b0001, 0b0001, 0b0001, 0b0001, 0b0001, 0b1110}}, // C
        {{0b0111, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0111}}, // D
        {{0b1111, 0b0001, 0b0001, 0b0111, 0b0001, 0b0001, 0b1111}}, // E
        {{0b1111, 0b0001, 0b0001, 0b0111, 0b0001, 0b0001, 0b0001}}, // F
        {{0b1110, 0b0001, 0b0001, 0b1101, 0b1001, 0b1001, 0b1110}}, // G
        {{0b1001, 0b1001, 0b1001, 0b1111, 0b1001, 0b1001, 0b1001}}, // H
        {{0b1110, 0b0100, 0b0100, 0b0100, 0b0100, 0b0100, 0b1110}}, // I
        {{0b1100, 0b1000, 0b1000, 0b1000, 0b1001, 0b1001, 0b0110}}, // J
        {{0b1001, 0b0101, 0b0011, 0b0011, 0b0101, 0b1001, 0b1001}}, // K
        {{0b0001, 0b0001, 0b0001, 0b0001, 0b0001, 0b0001, 0b1111}}, // L
        {{0b1001, 0b1111, 0b1111, 0b1001, 0b1001, 0b1001, 0b1001}}, // M
        {{0b1001, 0b1011, 0b1011, 0b1101, 0b1101, 0b1001, 0b1001}}, // N
        {{0b0110, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110}}, // O
        {{0b0111, 0b1001, 0b1001, 0b0111, 0b0001, 0b0001, 0b0001}}, // P
        {{0b0110, 0b1001, 0b1001, 0b1001, 0b0101, 0b1001, 0b1110}}, // Q
        {{0b0111, 0b1001, 0b1001, 0b0111, 0b0101, 0b1001, 0b1001}}, // R
        {{0b1110, 0b0001, 0b0001, 0b0110, 0b1000, 0b1000, 0b0111}}, // S
        {{0b1111, 0b0100, 0b0100, 0b0100, 0b0100, 0b0100, 0b0100}}, // T
        {{0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110}}, // U
        {{0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110, 0b0110}}, // V
        {{0b1001, 0b1001, 0b1001, 0b1001, 0b1111, 0b1111, 0b1001}}, // W
        {{0b1001, 0b1001, 0b0110, 0b0110, 0b0110, 0b1001, 0b1001}}, // X
        {{0b1001, 0b1001, 0b1001, 0b0110, 0b0100, 0b0100, 0b0100}}, // Y
        {{0b1111, 0b1000, 0b0100, 0b0100, 0b0010, 0b0001, 0b1111}}, // Z
    }};
    static constexpr Glyph percent{{0b1001, 0b1001, 0b1000, 0b0100, 0b0010, 0b1001, 0b1001}};
    static constexpr Glyph hyphen{{0, 0, 0, 0b1111, 0, 0, 0}};
    static constexpr Glyph dot{{0, 0, 0, 0, 0, 0, 0b0010}};
    static constexpr Glyph colon{{0, 0b0100, 0b0100, 0, 0b0100, 0b0100, 0}};
    static constexpr Glyph slash{{0b1000, 0b1000, 0b0100, 0b0100, 0b0010, 0b0010, 0b0001}};
    static constexpr Glyph underscore{{0, 0, 0, 0, 0, 0, 0b1111}};

    const auto upper = static_cast<char>(std::toupper(
        static_cast<unsigned char>(character)));
    if (upper >= '0' && upper <= '9') {
        return kDigits[static_cast<std::size_t>(upper - '0')];
    }
    if (upper >= 'A' && upper <= 'Z') {
        return letters[static_cast<std::size_t>(upper - 'A')];
    }
    switch (upper) {
        case ' ': return blank;
        case '%': return percent;
        case '-': return hyphen;
        case '.': return dot;
        case ':': return colon;
        case '/': return slash;
        case '_': return underscore;
        default: return unknown;
    }
}

}  // namespace

ClockFace::Frame ClockFace::render_time(int hour, int minute, int second,
                                        int weekday, bool blink_on) {
    Frame frame{};
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 ||
        second > 59 || weekday < 0 || weekday > 6) {
        return frame;
    }

    draw_weekday(frame, weekday);
    draw_meridiem(frame, hour);
    const int display_hour = hour == 0 ? 12 : (hour > 12 ? hour - 12 : hour);
    draw_digit(frame, 2, display_hour / 10);
    draw_digit(frame, 7, display_hour % 10);
    draw_digit(frame, 15, minute / 10);
    draw_digit(frame, 20, minute % 10);
    draw_second_progress(frame, second, blink_on);
    return frame;
}

ClockFace::Frame ClockFace::render_date(int month, int day, int weekday) {
    Frame frame{};
    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        weekday < 0 || weekday > 6) return frame;
    draw_weekday(frame, weekday);

    constexpr std::array<const char*, 12> kMonths{{
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC",
    }};
    const char* abbreviation = kMonths[static_cast<std::size_t>(month - 1)];
    for (int index = 0; index < 3; ++index) {
        const auto& glyph = text_glyph(abbreviation[index]);
        const int glyph_x = 2 + index * 5;
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 4; ++column) {
                if ((glyph[static_cast<std::size_t>(row)] &
                     (1u << column)) != 0) {
                    set_pixel(frame, glyph_x + column, row + 1);
                }
            }
        }
    }

    const int day_digits[2] = {day / 10, day % 10};
    for (int index = 0; index < 2; ++index) {
        const auto& glyph = kSmallDigits[static_cast<std::size_t>(day_digits[index])];
        const int glyph_x = 17 + index * 4;
        for (int row = 0; row < 5; ++row) {
            for (int column = 0; column < 3; ++column) {
                if ((glyph[static_cast<std::size_t>(row)] &
                     (1u << column)) != 0) {
                    set_pixel(frame, glyph_x + column, row + 2);
                }
            }
        }
    }
    return frame;
}

ClockFace::Frame ClockFace::render_temperature(float celsius, bool fahrenheit) {
    Frame frame{};
    float displayed = fahrenheit ? celsius * 9.0f / 5.0f + 32.0f : celsius;
    int rounded = static_cast<int>(displayed + (displayed >= 0 ? 0.5f : -0.5f));
    rounded = std::max(0, std::min(999, rounded));

    // Center the significant digits instead of padding an ordinary room
    // temperature with leading zeroes. The remaining space carries a compact
    // degree mark and a 3x5 unit glyph inside the main display area.
    const int digit_count = rounded >= 100 ? 3 : (rounded >= 10 ? 2 : 1);
    const int digit_start = digit_count == 3 ? 2 : (digit_count == 2 ? 5 : 7);
    int divisor = digit_count == 3 ? 100 : (digit_count == 2 ? 10 : 1);
    for (int index = 0; index < digit_count; ++index) {
        draw_digit(frame, digit_start + index * 5, (rounded / divisor) % 10);
        divisor /= 10;
    }

    const int degree_x = digit_start + digit_count * 5;
    for (const int y : {1, 2}) {
        set_pixel(frame, degree_x, y);
        set_pixel(frame, degree_x + 1, y);
    }

    constexpr std::array<std::uint8_t, 5> kSmallF{{
        0b111, 0b001, 0b111, 0b001, 0b001,
    }};
    constexpr std::array<std::uint8_t, 5> kSmallC{{
        0b111, 0b001, 0b001, 0b001, 0b111,
    }};
    const auto& unit = fahrenheit ? kSmallF : kSmallC;
    const int unit_x = degree_x + 3;
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column < 3; ++column) {
            if ((unit[static_cast<std::size_t>(row)] & (1u << column)) != 0) {
                set_pixel(frame, unit_x + column, row + 3);
            }
        }
    }

    // Waveshare's status bar has dedicated F and C LEDs on row 3.
    set_pixel(frame, fahrenheit ? 0 : 1, 3);
    return frame;
}

ClockFace::Frame ClockFace::render_reset_countdown(int remaining_seconds,
                                                   bool blink_on) {
    Frame frame{};
    const int bounded = std::max(0, std::min(10, remaining_seconds));
    draw_four_digits(frame, bounded);
    const int completed = (10 - bounded) * 6 / 10;
    draw_progress(frame, completed, completed < 6 && blink_on);
    return frame;
}

ClockFace::Frame ClockFace::render_text(const char* text,
                                        std::uint32_t scroll_step) {
    Frame frame{};
    if (text == nullptr || text[0] == '\0') return frame;

    constexpr int kContentLeft = 2;
    constexpr int kContentWidth = kWidth - kContentLeft;
    constexpr int kCharacterAdvance = 5;
    constexpr int kMarqueeGap = 5;
    const std::size_t length = std::min<std::size_t>(std::strlen(text), 96);
    const int text_width = static_cast<int>(length) * kCharacterAdvance - 1;
    int origin = kContentLeft;
    int cycle_width = 0;
    if (text_width <= kContentWidth) {
        origin += (kContentWidth - text_width) / 2;
    } else {
        cycle_width = text_width + kMarqueeGap;
        origin -= static_cast<int>(scroll_step %
                                   static_cast<std::uint32_t>(cycle_width));
    }

    const auto draw_copy = [&](int copy_origin) {
        for (std::size_t index = 0; index < length; ++index) {
            const int glyph_x = copy_origin +
                static_cast<int>(index) * kCharacterAdvance;
            const auto& glyph = text_glyph(text[index]);
            for (int row = 0; row < 7; ++row) {
                for (int column = 0; column < 4; ++column) {
                    const int x = glyph_x + column;
                    if (x >= kContentLeft && x < kWidth &&
                        (glyph[static_cast<std::size_t>(row)] &
                         (1u << column)) != 0) {
                        set_pixel(frame, x, row + 1);
                    }
                }
            }
        }
    };

    draw_copy(origin);
    if (cycle_width > 0) draw_copy(origin + cycle_width);
    return frame;
}

void ClockFace::apply_status_indicators(Frame& frame,
                                        bool automatic_brightness,
                                        bool chime_enabled) {
    // The printed Hourly Chime icon is the two-pixel status cell on row 6.
    for (const int x : {0, 1}) {
        if (chime_enabled) set_pixel(frame, x, 6);
        else clear_pixel(frame, x, 6);
    }
    // The printed Auto Light icon is the two-pixel status cell on row 7.
    for (const int x : {0, 1}) {
        if (automatic_brightness) set_pixel(frame, x, 7);
        else clear_pixel(frame, x, 7);
    }
}

void ClockFace::draw_weekday(Frame& frame, int weekday) {
    // Pico datetime_t uses 0=Sunday. The paired top-row LEDs follow the
    // physical order printed on the clock: Monday through Sunday.
    constexpr std::array<std::array<int, 2>, 7> kWeekdayColumns{{
        {{21, 22}},  // Sunday
        {{3, 4}},    // Monday
        {{6, 7}},    // Tuesday
        {{9, 10}},   // Wednesday
        {{12, 13}},  // Thursday
        {{15, 16}},  // Friday
        {{18, 19}},  // Saturday
    }};

    set_pixel(frame, kWeekdayColumns[static_cast<std::size_t>(weekday)][0], 0);
    set_pixel(frame, kWeekdayColumns[static_cast<std::size_t>(weekday)][1], 0);
}

void ClockFace::draw_meridiem(Frame& frame, int hour) {
    // The AM and PM indicators occupy the two reserved left-side columns.
    set_pixel(frame, hour < 12 ? 0 : 1, 4);
}

bool ClockFace::pixel(const Frame& frame, int x, int y) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return false;
    return (frame[static_cast<std::size_t>(y)] &
            (std::uint32_t{1} << (31 - x))) != 0;
}

void ClockFace::draw_digit(Frame& frame, int x, int digit) {
    if (digit < 0 || digit > 9) return;
    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 4; ++column) {
            if ((kDigits[static_cast<std::size_t>(digit)][static_cast<std::size_t>(row)] &
                 (1u << column)) != 0) {
                set_pixel(frame, x + column, row + 1);
            }
        }
    }
}

void ClockFace::draw_second_progress(Frame& frame, int second, bool blink_on) {
    draw_progress(frame, second / 10, (second % 10) != 0 && blink_on);
}

void ClockFace::draw_progress(Frame& frame, int completed, bool blink_current) {
    constexpr std::array<std::array<int, 2>, 6> kDots{{
        {{12, 2}}, {{13, 2}},
        {{12, 4}}, {{13, 4}},
        {{12, 6}}, {{13, 6}},
    }};

    const int bounded = std::max(0, std::min(6, completed));
    for (int index = 0; index < bounded; ++index) {
        set_pixel(frame, kDots[static_cast<std::size_t>(index)][0],
                  kDots[static_cast<std::size_t>(index)][1]);
    }

    if (blink_current && bounded < static_cast<int>(kDots.size())) {
        set_pixel(frame, kDots[static_cast<std::size_t>(bounded)][0],
                  kDots[static_cast<std::size_t>(bounded)][1]);
    }
}

void ClockFace::draw_four_digits(Frame& frame, int value) {
    value = std::max(0, std::min(9999, value));
    draw_digit(frame, 2, value / 1000);
    draw_digit(frame, 7, (value / 100) % 10);
    draw_digit(frame, 15, (value / 10) % 10);
    draw_digit(frame, 20, value % 10);
}

void ClockFace::set_pixel(Frame& frame, int x, int y) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
    frame[static_cast<std::size_t>(y)] |= std::uint32_t{1} << (31 - x);
}

void ClockFace::clear_pixel(Frame& frame, int x, int y) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
    frame[static_cast<std::size_t>(y)] &=
        ~(std::uint32_t{1} << (31 - x));
}

}  // namespace pico_clock
