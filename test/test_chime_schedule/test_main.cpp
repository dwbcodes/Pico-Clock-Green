#include <unity.h>

#include <initializer_list>

#include "chime_schedule.hpp"

using pico_clock::chime_due;
using pico_clock::chime_interval_name;
using pico_clock::parse_chime_interval;
using pico_clock::valid_chime_interval;

void setUp() {}
void tearDown() {}

void test_chime_intervals_have_stable_api_names() {
    std::uint8_t minutes = 99;
    for (const auto expected : {0, 15, 30, 60}) {
        TEST_ASSERT_TRUE(valid_chime_interval(expected));
        TEST_ASSERT_TRUE(parse_chime_interval(chime_interval_name(expected), minutes));
        TEST_ASSERT_EQUAL_UINT8(expected, minutes);
    }
    TEST_ASSERT_FALSE(valid_chime_interval(10));
    TEST_ASSERT_FALSE(parse_chime_interval("hourly", minutes));
}

void test_chime_is_due_only_on_selected_boundaries() {
    TEST_ASSERT_FALSE(chime_due(0, 0, 0));
    TEST_ASSERT_TRUE(chime_due(15, 15, 0));
    TEST_ASSERT_TRUE(chime_due(15, 45, 0));
    TEST_ASSERT_FALSE(chime_due(15, 16, 0));
    TEST_ASSERT_TRUE(chime_due(30, 30, 0));
    TEST_ASSERT_FALSE(chime_due(30, 15, 0));
    TEST_ASSERT_TRUE(chime_due(60, 0, 0));
    TEST_ASSERT_FALSE(chime_due(60, 30, 0));
    TEST_ASSERT_FALSE(chime_due(15, 15, 1));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_chime_intervals_have_stable_api_names);
    RUN_TEST(test_chime_is_due_only_on_selected_boundaries);
    return UNITY_END();
}
