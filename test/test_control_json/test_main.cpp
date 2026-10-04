#include <unity.h>

#include <cstdio>

#include "control_json.hpp"

using namespace pico_clock;

namespace {

void test_all_documented_display_actions_parse() {
    const char* names[] = {
        "next-page", "show-time", "show-date", "show-temperature",
        "show-status", "next-status", "previous-status",
        "brightness-up", "brightness-down",
        "automatic-brightness", "select-celsius", "select-fahrenheit", "wake",
        "sleep-until-schedule", "maximum-brightness",
    };
    for (const auto* name : names) {
        char body[80];
        std::snprintf(body, sizeof(body), "{\"action\":\"%s\"}", name);
        ControlAction action{};
        TEST_ASSERT_TRUE_MESSAGE(ControlJson::parse_display_action(body, action), name);
    }
}

void test_factory_reset_requires_exact_confirmation() {
    TEST_ASSERT_TRUE(ControlJson::confirms_factory_reset(
        "{\"confirmation\":\"erase-all-settings\"}"));
    TEST_ASSERT_FALSE(ControlJson::confirms_factory_reset(
        "{\"confirmation\":\"yes\"}"));
    TEST_ASSERT_FALSE(ControlJson::confirms_factory_reset("{}"));
}

void test_power_save_test_accepts_only_documented_durations() {
    unsigned seconds = 99;
    TEST_ASSERT_TRUE(ControlJson::parse_power_save_test(
        "{\"durationSeconds\":0}", seconds));
    TEST_ASSERT_EQUAL_UINT(0, seconds);
    TEST_ASSERT_TRUE(ControlJson::parse_power_save_test(
        "{ \"durationSeconds\" : 15 }", seconds));
    TEST_ASSERT_EQUAL_UINT(15, seconds);
    TEST_ASSERT_FALSE(ControlJson::parse_power_save_test(
        "{\"durationSeconds\":1}", seconds));
    TEST_ASSERT_FALSE(ControlJson::parse_power_save_test("{}", seconds));
    TEST_ASSERT_FALSE(ControlJson::parse_power_save_test(
        "{\"durationSeconds\":\"5\"}", seconds));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_all_documented_display_actions_parse);
    RUN_TEST(test_factory_reset_requires_exact_confirmation);
    RUN_TEST(test_power_save_test_accepts_only_documented_durations);
    return UNITY_END();
}
