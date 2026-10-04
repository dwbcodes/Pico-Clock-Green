#include <unity.h>

#include "display_schedule.hpp"

using pico_clock::DisplaySchedule;

namespace {

void test_parse_and_format_time() {
    std::uint16_t minute = 0;
    TEST_ASSERT_TRUE(DisplaySchedule::parse_time("22:30", minute));
    TEST_ASSERT_EQUAL(1350, minute);
    char output[6]{};
    TEST_ASSERT_TRUE(DisplaySchedule::format_time(minute, output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("22:30", output);
}

void test_rejects_invalid_times() {
    std::uint16_t minute = 0;
    TEST_ASSERT_FALSE(DisplaySchedule::parse_time("24:00", minute));
    TEST_ASSERT_FALSE(DisplaySchedule::parse_time("12:60", minute));
    TEST_ASSERT_FALSE(DisplaySchedule::parse_time("9:00", minute));
}

void test_disabled_schedule_is_always_on() {
    TEST_ASSERT_TRUE(DisplaySchedule::display_on(false, 1320, 420, 2, 0));
}

void test_overnight_off_period() {
    TEST_ASSERT_FALSE(DisplaySchedule::display_on(true, 1320, 420, 22, 0));
    TEST_ASSERT_FALSE(DisplaySchedule::display_on(true, 1320, 420, 6, 59));
    TEST_ASSERT_TRUE(DisplaySchedule::display_on(true, 1320, 420, 7, 0));
    TEST_ASSERT_TRUE(DisplaySchedule::display_on(true, 1320, 420, 21, 59));
}

void test_daytime_off_period() {
    TEST_ASSERT_TRUE(DisplaySchedule::display_on(true, 480, 1020, 7, 59));
    TEST_ASSERT_FALSE(DisplaySchedule::display_on(true, 480, 1020, 8, 0));
    TEST_ASSERT_TRUE(DisplaySchedule::display_on(true, 480, 1020, 17, 0));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_and_format_time);
    RUN_TEST(test_rejects_invalid_times);
    RUN_TEST(test_disabled_schedule_is_always_on);
    RUN_TEST(test_overnight_off_period);
    RUN_TEST(test_daytime_off_period);
    return UNITY_END();
}
