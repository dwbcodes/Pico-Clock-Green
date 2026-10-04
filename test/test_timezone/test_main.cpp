#include <unity.h>

#include "timezone.hpp"

using pico_clock::LocalTime;
using pico_clock::Timezone;

namespace {

void test_supported_names_are_explicit() {
    TEST_ASSERT_TRUE(Timezone::supported("UTC"));
    TEST_ASSERT_TRUE(Timezone::supported("America/Los_Angeles"));
    TEST_ASSERT_TRUE(Timezone::supported("Europe/London"));
    TEST_ASSERT_FALSE(Timezone::supported("PST"));
    TEST_ASSERT_FALSE(Timezone::supported("America/Unknown"));
}

void test_los_angeles_uses_standard_time_in_winter() {
    LocalTime local{};
    TEST_ASSERT_TRUE(Timezone::to_local(1767286800u, "America/Los_Angeles", local));
    TEST_ASSERT_EQUAL(2026, local.year);
    TEST_ASSERT_EQUAL(1, local.month);
    TEST_ASSERT_EQUAL(1, local.day);
    TEST_ASSERT_EQUAL(9, local.hour);
    TEST_ASSERT_EQUAL(-480, local.utc_offset_minutes);
    TEST_ASSERT_FALSE(local.daylight_saving);
}

void test_los_angeles_uses_daylight_time_in_summer() {
    LocalTime local{};
    TEST_ASSERT_TRUE(Timezone::to_local(1782925200u, "America/Los_Angeles", local));
    TEST_ASSERT_EQUAL(10, local.hour);
    TEST_ASSERT_EQUAL(-420, local.utc_offset_minutes);
    TEST_ASSERT_TRUE(local.daylight_saving);
}

void test_us_dst_boundaries_use_the_correct_offset() {
    LocalTime before{};
    LocalTime after{};
    TEST_ASSERT_TRUE(Timezone::to_local(1772963999u, "America/Los_Angeles", before));
    TEST_ASSERT_TRUE(Timezone::to_local(1772964000u, "America/Los_Angeles", after));
    TEST_ASSERT_EQUAL(1, before.hour);
    TEST_ASSERT_EQUAL(59, before.minute);
    TEST_ASSERT_EQUAL(3, after.hour);
    TEST_ASSERT_EQUAL(0, after.minute);
}

void test_unsupported_zone_does_not_convert() {
    LocalTime local{};
    TEST_ASSERT_FALSE(Timezone::to_local(0, "Mars/Olympus", local));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_supported_names_are_explicit);
    RUN_TEST(test_los_angeles_uses_standard_time_in_winter);
    RUN_TEST(test_los_angeles_uses_daylight_time_in_summer);
    RUN_TEST(test_us_dst_boundaries_use_the_correct_offset);
    RUN_TEST(test_unsupported_zone_does_not_convert);
    return UNITY_END();
}
