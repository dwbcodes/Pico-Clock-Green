#include <unity.h>

#include "clock_control.hpp"

using namespace pico_clock;

namespace {

void test_pages_cycle_and_return_to_time() {
    ClockControl control;
    control.apply(ControlAction::NextPage, 100);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayPage::Date),
                          static_cast<int>(control.status(100).page));
    control.tick(8099);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayPage::Date),
                          static_cast<int>(control.status(8099).page));
    control.tick(8100);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayPage::Time),
                          static_cast<int>(control.status(8100).page));
}

void test_schedule_transition_clears_sleep_override() {
    ClockControl control;
    control.apply(ControlAction::SleepUntilSchedule, 10);
    TEST_ASSERT_FALSE(control.display_enabled(true, 10));
    control.schedule_state(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayOverride::Automatic),
        static_cast<int>(control.status(10).display_override));
}

void test_control_effects_are_one_shot() {
    ClockControl control;
    control.apply(ControlAction::SynchronizeTime, 0);
    control.apply(ControlAction::StartProvisioning, 0);
    auto effects = control.take_effects();
    TEST_ASSERT_TRUE(effects.synchronize_time);
    TEST_ASSERT_TRUE(effects.start_provisioning);
    effects = control.take_effects();
    TEST_ASSERT_FALSE(effects.synchronize_time);
    TEST_ASSERT_FALSE(effects.start_provisioning);
}

void test_button_chords_request_recovery_actions() {
    ClockControl provisioning;
    provisioning.update_buttons(false, true, true, 100);
    provisioning.update_buttons(false, true, true, 5100);
    TEST_ASSERT_TRUE(provisioning.take_effects().start_provisioning);

    ClockControl reset;
    reset.update_buttons(true, true, true, 100);
    TEST_ASSERT_EQUAL_UINT8(10,
        reset.status(100).factory_reset_countdown_seconds);
    reset.update_buttons(true, true, true, 5100);
    TEST_ASSERT_EQUAL_UINT8(5,
        reset.status(5100).factory_reset_countdown_seconds);
    reset.update_buttons(true, true, true, 10100);
    TEST_ASSERT_TRUE(reset.take_effects().factory_reset);
}

void test_any_button_wakes_a_sleep_override() {
    ClockControl control;
    control.apply(ControlAction::SleepUntilSchedule, 0);
    TEST_ASSERT_FALSE(control.display_enabled(true, 1));
    control.update_buttons(false, true, false, 100);
    TEST_ASSERT_TRUE(control.display_enabled(false, 100));
    control.update_buttons(false, false, false, 150);

}

void test_ambient_brightness_uses_bias_and_maximum_override() {
    ClockControl control;
    TEST_ASSERT_EQUAL_UINT8(1, control.brightness(0, 100));
    TEST_ASSERT_EQUAL_UINT8(10, control.brightness(4095, 100));
    control.apply(ControlAction::BrightnessUp, 100);
    TEST_ASSERT_EQUAL_UINT8(2, control.brightness(0, 100));
    control.apply(ControlAction::TemporaryMaximumBrightness, 200);
    TEST_ASSERT_EQUAL_UINT8(10, control.brightness(0, 201));
    TEST_ASSERT_EQUAL_UINT8(2, control.brightness(0, 30200));

    control.configure_brightness(false, 70);
    TEST_ASSERT_FALSE(control.status(30200).automatic_brightness);
    TEST_ASSERT_EQUAL_UINT8(70,
        control.status(30200).manual_brightness_percent);
    // The existing +1 button bias is retained over the configured 70% base.
    TEST_ASSERT_EQUAL_UINT8(8, control.brightness(0, 30200));
}

void test_temperature_buttons_select_explicit_units() {
    ClockControl control;
    control.apply(ControlAction::ShowTemperature, 0);

    control.update_buttons(false, false, true, 100);
    control.update_buttons(false, false, false, 150);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TemperatureUnit::Celsius),
                          static_cast<int>(control.status(150).temperature_unit));

    // Re-selecting C is idempotent rather than toggling back to F.
    control.update_buttons(false, false, true, 200);
    control.update_buttons(false, false, false, 250);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TemperatureUnit::Celsius),
                          static_cast<int>(control.status(250).temperature_unit));

    control.update_buttons(false, true, false, 300);
    control.update_buttons(false, false, false, 350);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TemperatureUnit::Fahrenheit),
                          static_cast<int>(control.status(350).temperature_unit));
}

void test_status_buttons_select_wifi_and_ntp_views() {
    ClockControl control;
    control.apply(ControlAction::ShowStatus, 0);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayPage::Status),
                          static_cast<int>(control.status(0).page));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StatusView::Wifi),
                          static_cast<int>(control.status(0).status_view));

    control.update_buttons(false, true, false, 100);
    control.update_buttons(false, false, false, 150);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StatusView::Ntp),
                          static_cast<int>(control.status(150).status_view));
    TEST_ASSERT_EQUAL_UINT32(150, control.status(150).status_view_started_ms);

    control.update_buttons(false, true, false, 160);
    control.update_buttons(false, false, false, 190);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StatusView::Brightness),
                          static_cast<int>(control.status(190).status_view));

    control.update_buttons(false, false, true, 200);
    control.update_buttons(false, false, false, 250);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StatusView::Ntp),
                          static_cast<int>(control.status(250).status_view));

    // Status remains visible while it is being inspected and scrolled.
    control.tick(60000);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayPage::Status),
                          static_cast<int>(control.status(60000).page));

    control.apply(ControlAction::ShowStatus, 70000);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StatusView::Wifi),
                          static_cast<int>(control.status(70000).status_view));
    TEST_ASSERT_EQUAL_UINT32(70000,
                             control.status(70000).status_view_started_ms);
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_pages_cycle_and_return_to_time);
    RUN_TEST(test_schedule_transition_clears_sleep_override);
    RUN_TEST(test_control_effects_are_one_shot);
    RUN_TEST(test_button_chords_request_recovery_actions);
    RUN_TEST(test_any_button_wakes_a_sleep_override);
    RUN_TEST(test_ambient_brightness_uses_bias_and_maximum_override);
    RUN_TEST(test_temperature_buttons_select_explicit_units);
    RUN_TEST(test_status_buttons_select_wifi_and_ntp_views);
    return UNITY_END();
}
