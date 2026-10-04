#include <cstring>

#include <unity.h>

#include "clock_time.hpp"

using pico_clock::ClockTime;
using pico_clock::clock_time_difference_seconds;
using pico_clock::format_clock_time_json;
using pico_clock::format_clock_time_iso_json_value;

void setUp() {}
void tearDown() {}

void test_formats_valid_clock_time() {
    const ClockTime time{true, 2026, 10, 4, 0, 9, 7, 5};
    char output[256]{};
    TEST_ASSERT_TRUE(format_clock_time_json(
        time, "America/Los_Angeles", true, output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING(
        "{\"valid\":true,\"date\":\"2026-10-04\",\"time\":\"09:07:05\","
        "\"weekday\":0,\"timezone\":\"America/Los_Angeles\","
        "\"ntpSynchronized\":true}\n",
        output);
}

void test_formats_unavailable_clock_time_with_null_values() {
    const ClockTime time{};
    char output[192]{};
    TEST_ASSERT_TRUE(format_clock_time_json(
        time, "UTC", false, output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING(
        "{\"valid\":false,\"date\":null,\"time\":null,\"weekday\":null,"
        "\"timezone\":\"UTC\",\"ntpSynchronized\":false}\n",
        output);
}

void test_rejects_a_too_small_output_buffer() {
    const ClockTime time{true, 2026, 10, 4, 0, 9, 7, 5};
    char output[16]{};
    TEST_ASSERT_FALSE(format_clock_time_json(
        time, "UTC", true, output, sizeof(output)));
}

void test_formats_an_iso_timestamp_json_value() {
    const ClockTime time{true, 2026, 10, 4, 0, 9, 7, 5};
    char output[48]{};
    TEST_ASSERT_TRUE(format_clock_time_iso_json_value(
        time, output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("\"2026-10-04T09:07:05\"", output);
}

void test_iso_timestamp_rejects_invalid_values_and_truncation() {
    const ClockTime invalid{true, 2026, 13, 4, 0, 9, 7, 5};
    char output[48]{};
    TEST_ASSERT_TRUE(format_clock_time_iso_json_value(
        invalid, output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("null", output);

    const ClockTime time{true, 2026, 10, 4, 0, 9, 7, 5};
    char too_small[8]{};
    TEST_ASSERT_FALSE(format_clock_time_iso_json_value(
        time, too_small, sizeof(too_small)));
}

void test_measures_rtc_skew_across_calendar_boundaries() {
    const ClockTime ntp{true, 2026, 10, 4, 0, 23, 59, 58};
    const ClockTime rtc_ahead{true, 2026, 10, 5, 1, 0, 0, 3};
    std::int32_t difference = 0;
    TEST_ASSERT_TRUE(clock_time_difference_seconds(rtc_ahead, ntp, difference));
    TEST_ASSERT_EQUAL_INT32(5, difference);

    const ClockTime rtc_behind{true, 2026, 10, 4, 0, 23, 59, 55};
    TEST_ASSERT_TRUE(clock_time_difference_seconds(rtc_behind, ntp, difference));
    TEST_ASSERT_EQUAL_INT32(-3, difference);
    TEST_ASSERT_FALSE(clock_time_difference_seconds(ClockTime{}, ntp, difference));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_formats_valid_clock_time);
    RUN_TEST(test_formats_unavailable_clock_time_with_null_values);
    RUN_TEST(test_rejects_a_too_small_output_buffer);
    RUN_TEST(test_formats_an_iso_timestamp_json_value);
    RUN_TEST(test_iso_timestamp_rejects_invalid_values_and_truncation);
    RUN_TEST(test_measures_rtc_skew_across_calendar_boundaries);
    return UNITY_END();
}
