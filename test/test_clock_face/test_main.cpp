#include <unity.h>

#include "clock_face.hpp"

using pico_clock::ClockFace;

namespace {

void test_invalid_time_is_blank() {
    const auto frame = ClockFace::render_time(24, 0, 0, 0, false);
    for (const auto row : frame) TEST_ASSERT_EQUAL_UINT32(0, row);
}

void test_date_temperature_and_reset_countdown_render() {
    const auto date = ClockFace::render_date(10, 4, 0);
    TEST_ASSERT_TRUE(ClockFace::pixel(date, 21, 0));
    TEST_ASSERT_TRUE(ClockFace::pixel(date, 22, 0));

    const auto temperature = ClockFace::render_temperature(20.0f, true);
    TEST_ASSERT_TRUE(ClockFace::pixel(temperature, 0, 3));
    TEST_ASSERT_FALSE(ClockFace::pixel(temperature, 1, 3));

    const auto reset = ClockFace::render_reset_countdown(5, false);
    TEST_ASSERT_TRUE(ClockFace::pixel(reset, 12, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(reset, 13, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(reset, 12, 4));
}

void test_date_uses_three_letter_month_and_compact_day() {
    const auto october = ClockFace::render_date(10, 4, 0);

    // OCT uses three 4x7 letters at x=2, 7, and 12.
    TEST_ASSERT_TRUE(ClockFace::pixel(october, 3, 1));
    TEST_ASSERT_TRUE(ClockFace::pixel(october, 4, 1));
    TEST_ASSERT_TRUE(ClockFace::pixel(october, 7, 2));
    for (int x = 12; x <= 15; ++x) {
        TEST_ASSERT_TRUE(ClockFace::pixel(october, x, 1));
    }

    // The day 04 is a smaller 3x5 pair at x=17..19 and x=21..23.
    for (int x = 17; x <= 19; ++x) {
        TEST_ASSERT_TRUE(ClockFace::pixel(october, x, 2));
    }
    TEST_ASSERT_TRUE(ClockFace::pixel(october, 21, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(october, 23, 2));
    TEST_ASSERT_FALSE(ClockFace::pixel(october, 22, 2));

    // Month selection changes the rendered abbreviation rather than showing
    // a numeric month with a leading zero.
    const auto january = ClockFace::render_date(1, 4, 0);
    TEST_ASSERT_TRUE(ClockFace::pixel(october, 3, 1) !=
                     ClockFace::pixel(january, 3, 1));
}

void test_temperature_uses_dedicated_f_and_c_icons() {
    const auto fahrenheit = ClockFace::render_temperature(20.0f, true);
    TEST_ASSERT_TRUE(ClockFace::pixel(fahrenheit, 0, 3));
    TEST_ASSERT_FALSE(ClockFace::pixel(fahrenheit, 1, 3));
    TEST_ASSERT_FALSE(ClockFace::pixel(fahrenheit, 0, 4));
    TEST_ASSERT_FALSE(ClockFace::pixel(fahrenheit, 1, 4));

    const auto celsius = ClockFace::render_temperature(20.0f, false);
    TEST_ASSERT_FALSE(ClockFace::pixel(celsius, 0, 3));
    TEST_ASSERT_TRUE(ClockFace::pixel(celsius, 1, 3));
}

void test_temperature_is_centered_without_leading_zeroes_and_has_degree_unit() {
    const auto fahrenheit = ClockFace::render_temperature(30.0f, true);  // 86 F

    // Two 4x7 digits occupy x=5..8 and x=10..13. The old leading-zero
    // positions at the far left of the numeric area remain empty.
    for (int y = 1; y < ClockFace::kHeight; ++y) {
        for (int x = 2; x <= 4; ++x) {
            TEST_ASSERT_FALSE(ClockFace::pixel(fahrenheit, x, y));
        }
    }
    TEST_ASSERT_TRUE(ClockFace::pixel(fahrenheit, 5, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(fahrenheit, 8, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(fahrenheit, 12, 1));
    TEST_ASSERT_FALSE(ClockFace::pixel(fahrenheit, 11, 1));

    // A compact 2x2 degree mark is followed by the 3x5 F glyph.
    for (const int y : {1, 2}) {
        TEST_ASSERT_TRUE(ClockFace::pixel(fahrenheit, 15, y));
        TEST_ASSERT_TRUE(ClockFace::pixel(fahrenheit, 16, y));
    }
    TEST_ASSERT_TRUE(ClockFace::pixel(fahrenheit, 18, 3));
    TEST_ASSERT_TRUE(ClockFace::pixel(fahrenheit, 20, 3));
    TEST_ASSERT_TRUE(ClockFace::pixel(fahrenheit, 18, 7));
    TEST_ASSERT_FALSE(ClockFace::pixel(fahrenheit, 20, 7));

    const auto celsius = ClockFace::render_temperature(30.0f, false);
    TEST_ASSERT_TRUE(ClockFace::pixel(celsius, 18, 3));
    TEST_ASSERT_TRUE(ClockFace::pixel(celsius, 20, 3));
    TEST_ASSERT_TRUE(ClockFace::pixel(celsius, 18, 7));
    TEST_ASSERT_TRUE(ClockFace::pixel(celsius, 20, 7));
}

void test_auto_light_icon_tracks_automatic_brightness() {
    auto frame = ClockFace::render_time(12, 34, 0, 0, false);
    ClockFace::apply_status_indicators(frame, true);
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 0, 7));
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 1, 7));

    ClockFace::apply_status_indicators(frame, false);
    TEST_ASSERT_FALSE(ClockFace::pixel(frame, 0, 7));
    TEST_ASSERT_FALSE(ClockFace::pixel(frame, 1, 7));
}

void test_chime_icon_tracks_configured_interval() {
    auto frame = ClockFace::render_time(12, 34, 0, 0, false);
    ClockFace::apply_status_indicators(frame, true, true);
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 0, 6));
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 1, 6));

    ClockFace::apply_status_indicators(frame, true, false);
    TEST_ASSERT_FALSE(ClockFace::pixel(frame, 0, 6));
    TEST_ASSERT_FALSE(ClockFace::pixel(frame, 1, 6));
}

void test_short_text_is_centered_and_long_text_scrolls() {
    const auto short_text = ClockFace::render_text("NTP", 99);
    TEST_ASSERT_TRUE(ClockFace::pixel(short_text, 6, 1));
    TEST_ASSERT_TRUE(ClockFace::pixel(short_text, 18, 1));

    const auto first = ClockFace::render_text("WIFI NETWORK 75%", 0);
    const auto shifted = ClockFace::render_text("WIFI NETWORK 75%", 1);
    bool different = false;
    for (int y = 0; y < ClockFace::kHeight; ++y) {
        for (int x = 2; x < ClockFace::kWidth; ++x) {
            if (ClockFace::pixel(first, x, y) !=
                ClockFace::pixel(shifted, x, y)) different = true;
        }
    }
    TEST_ASSERT_TRUE(different);
    for (int y = 0; y < ClockFace::kHeight; ++y) {
        TEST_ASSERT_FALSE(ClockFace::pixel(first, 0, y));
        TEST_ASSERT_FALSE(ClockFace::pixel(first, 1, y));
    }
}

void test_time_reaches_right_edge_and_reserves_unused_status_pixels() {
    const auto frame = ClockFace::render_time(0, 0, 0, 0, false);
    for (int y = 0; y < ClockFace::kHeight; ++y) {
        if (y != 4) {
            TEST_ASSERT_FALSE(ClockFace::pixel(frame, 0, y));
            TEST_ASSERT_FALSE(ClockFace::pixel(frame, 1, y));
        }
    }
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 23, 2));
}

void test_digits_match_physical_cell_widths_and_spacing() {
    const auto frame = ClockFace::render_time(8, 8, 0, 0, false);

    // All four cells are four columns wide: 2..5, 7..10, 15..18, 20..23.
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 2, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 5, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 7, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 10, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 15, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 18, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 20, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 23, 2));

    for (const int separator : {6, 11, 12, 13, 14, 19}) {
        TEST_ASSERT_FALSE(ClockFace::pixel(frame, separator, 2));
    }
}

void test_second_progress_blinks_then_becomes_solid() {
    const auto blink_off = ClockFace::render_time(12, 34, 9, 0, false);
    const auto blink_on = ClockFace::render_time(12, 34, 9, 0, true);
    TEST_ASSERT_FALSE(ClockFace::pixel(blink_off, 12, 2));
    TEST_ASSERT_TRUE(ClockFace::pixel(blink_on, 12, 2));

    const auto tenth = ClockFace::render_time(12, 34, 10, 0, false);
    TEST_ASSERT_TRUE(ClockFace::pixel(tenth, 12, 2));
    TEST_ASSERT_FALSE(ClockFace::pixel(tenth, 13, 2));
}

void test_second_progress_advances_left_to_right_top_to_bottom() {
    constexpr int x[] = {12, 13, 12, 13, 12, 13};
    constexpr int y[] = {2, 2, 4, 4, 6, 6};

    for (int completed = 1; completed <= 5; ++completed) {
        const auto frame = ClockFace::render_time(12, 34, completed * 10, 0, false);
        for (int index = 0; index < 6; ++index) {
            TEST_ASSERT_EQUAL(index < completed,
                              ClockFace::pixel(frame, x[index], y[index]));
        }
    }

    const auto fifty_nine = ClockFace::render_time(12, 34, 59, 0, true);
    for (int index = 0; index < 6; ++index) {
        TEST_ASSERT_TRUE(ClockFace::pixel(fifty_nine, x[index], y[index]));
    }

    const auto next_minute = ClockFace::render_time(12, 35, 0, 0, true);
    for (int index = 0; index < 6; ++index) {
        TEST_ASSERT_FALSE(ClockFace::pixel(next_minute, x[index], y[index]));
    }
}

void test_midnight_displays_twelve_with_four_digits() {
    const auto frame = ClockFace::render_time(0, 0, 0, 0, false);
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 4, 1));
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 8, 1));
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 16, 1));
    TEST_ASSERT_TRUE(ClockFace::pixel(frame, 22, 7));
}

void test_pixel_rejects_out_of_bounds_coordinates() {
    const auto frame = ClockFace::render_time(12, 34, 0, 0, false);
    TEST_ASSERT_FALSE(ClockFace::pixel(frame, -1, 0));
    TEST_ASSERT_FALSE(ClockFace::pixel(frame, ClockFace::kWidth, 0));
    TEST_ASSERT_FALSE(ClockFace::pixel(frame, 0, -1));
    TEST_ASSERT_FALSE(ClockFace::pixel(frame, 0, ClockFace::kHeight));
}

void test_weekday_uses_documented_top_row_icon_pairs() {
    constexpr int columns[7][2] = {
        {21, 22}, {3, 4}, {6, 7}, {9, 10},
        {12, 13}, {15, 16}, {18, 19},
    };

    for (int weekday = 0; weekday < 7; ++weekday) {
        const auto frame = ClockFace::render_time(8, 0, 0, weekday, false);
        for (int candidate = 0; candidate < 7; ++candidate) {
            TEST_ASSERT_EQUAL(candidate == weekday,
                              ClockFace::pixel(frame, columns[candidate][0], 0));
            TEST_ASSERT_EQUAL(candidate == weekday,
                              ClockFace::pixel(frame, columns[candidate][1], 0));
        }
    }
}

void test_meridiem_uses_left_side_am_and_pm_icons() {
    const auto midnight = ClockFace::render_time(0, 0, 0, 0, false);
    TEST_ASSERT_TRUE(ClockFace::pixel(midnight, 0, 4));
    TEST_ASSERT_FALSE(ClockFace::pixel(midnight, 1, 4));

    const auto morning = ClockFace::render_time(11, 59, 0, 0, false);
    TEST_ASSERT_TRUE(ClockFace::pixel(morning, 0, 4));
    TEST_ASSERT_FALSE(ClockFace::pixel(morning, 1, 4));

    const auto noon = ClockFace::render_time(12, 0, 0, 0, false);
    TEST_ASSERT_FALSE(ClockFace::pixel(noon, 0, 4));
    TEST_ASSERT_TRUE(ClockFace::pixel(noon, 1, 4));

    const auto evening = ClockFace::render_time(23, 59, 0, 0, false);
    TEST_ASSERT_FALSE(ClockFace::pixel(evening, 0, 4));
    TEST_ASSERT_TRUE(ClockFace::pixel(evening, 1, 4));
}

void test_afternoon_hours_are_rendered_in_twelve_hour_format() {
    const auto one_am = ClockFace::render_time(1, 23, 0, 0, false);
    const auto one_pm = ClockFace::render_time(13, 23, 0, 0, false);
    for (int y = 1; y < ClockFace::kHeight; ++y) {
        for (int x = 2; x <= 10; ++x) {
            TEST_ASSERT_EQUAL(ClockFace::pixel(one_am, x, y),
                              ClockFace::pixel(one_pm, x, y));
        }
    }
}

void test_invalid_weekday_is_blank() {
    const auto frame = ClockFace::render_time(12, 34, 0, 7, false);
    for (const auto row : frame) TEST_ASSERT_EQUAL_UINT32(0, row);
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_invalid_time_is_blank);
    RUN_TEST(test_date_temperature_and_reset_countdown_render);
    RUN_TEST(test_date_uses_three_letter_month_and_compact_day);
    RUN_TEST(test_temperature_uses_dedicated_f_and_c_icons);
    RUN_TEST(test_temperature_is_centered_without_leading_zeroes_and_has_degree_unit);
    RUN_TEST(test_auto_light_icon_tracks_automatic_brightness);
    RUN_TEST(test_chime_icon_tracks_configured_interval);
    RUN_TEST(test_short_text_is_centered_and_long_text_scrolls);
    RUN_TEST(test_time_reaches_right_edge_and_reserves_unused_status_pixels);
    RUN_TEST(test_digits_match_physical_cell_widths_and_spacing);
    RUN_TEST(test_second_progress_blinks_then_becomes_solid);
    RUN_TEST(test_second_progress_advances_left_to_right_top_to_bottom);
    RUN_TEST(test_midnight_displays_twelve_with_four_digits);
    RUN_TEST(test_pixel_rejects_out_of_bounds_coordinates);
    RUN_TEST(test_weekday_uses_documented_top_row_icon_pairs);
    RUN_TEST(test_meridiem_uses_left_side_am_and_pm_icons);
    RUN_TEST(test_afternoon_hours_are_rendered_in_twelve_hour_format);
    RUN_TEST(test_invalid_weekday_is_blank);
    return UNITY_END();
}
