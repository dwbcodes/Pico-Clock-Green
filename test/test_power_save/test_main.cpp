#include <unity.h>

#include "power_save.hpp"

using namespace pico_clock;

namespace {

void test_schedule_off_enters_and_schedule_on_leaves_power_save() {
    PowerSaveController power;
    power.update(false, false, 100);
    TEST_ASSERT_TRUE(power.status(100).active);
    TEST_ASSERT_FALSE(power.display_enabled(false));

    power.update(true, false, 200);
    TEST_ASSERT_FALSE(power.status(200).active);
    TEST_ASSERT_TRUE(power.display_enabled(true));
}

void test_timed_test_blanks_then_wakes_at_deadline() {
    PowerSaveController power;
    TEST_ASSERT_TRUE(power.start_test(5, 1000));
    power.update(true, false, 1000);
    TEST_ASSERT_TRUE(power.status(1000).active);
    TEST_ASSERT_TRUE(power.status(1000).test_active);
    TEST_ASSERT_EQUAL_UINT8(5, power.status(1000).test_seconds_remaining);
    TEST_ASSERT_FALSE(power.display_enabled(true));

    power.update(true, false, 5999);
    TEST_ASSERT_TRUE(power.status(5999).test_active);
    power.update(true, false, 6000);
    TEST_ASSERT_FALSE(power.status(6000).active);
    TEST_ASSERT_FALSE(power.status(6000).test_active);
    TEST_ASSERT_TRUE(power.display_enabled(true));
}

void test_indefinite_test_only_ends_on_button() {
    PowerSaveController power;
    TEST_ASSERT_TRUE(power.start_test(0, 1000));
    power.update(true, false, 1000000);
    TEST_ASSERT_TRUE(power.status(1000000).test_active);
    TEST_ASSERT_EQUAL_UINT8(0, power.status(1000000).test_seconds_remaining);

    power.update(true, true, 1000020);
    TEST_ASSERT_FALSE(power.status(1000020).active);
    TEST_ASSERT_FALSE(power.status(1000020).test_active);
}

void test_only_documented_test_durations_are_accepted() {
    TEST_ASSERT_TRUE(PowerSaveController::valid_test_duration(0));
    TEST_ASSERT_TRUE(PowerSaveController::valid_test_duration(5));
    TEST_ASSERT_TRUE(PowerSaveController::valid_test_duration(10));
    TEST_ASSERT_TRUE(PowerSaveController::valid_test_duration(15));
    TEST_ASSERT_FALSE(PowerSaveController::valid_test_duration(1));
    TEST_ASSERT_FALSE(PowerSaveController::valid_test_duration(60));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_schedule_off_enters_and_schedule_on_leaves_power_save);
    RUN_TEST(test_timed_test_blanks_then_wakes_at_deadline);
    RUN_TEST(test_indefinite_test_only_ends_on_button);
    RUN_TEST(test_only_documented_test_durations_are_accepted);
    return UNITY_END();
}
