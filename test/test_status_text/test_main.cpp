#include <unity.h>

#include <cstring>

#include "status_text.hpp"

using pico_clock::StatusText;

namespace {

void test_wifi_status_contains_ssid_and_bounded_strength() {
    char text[96]{};
    TEST_ASSERT_TRUE(StatusText::wifi(text, sizeof(text), "Workshop", 73, true));
    TEST_ASSERT_EQUAL_STRING("WIFI Workshop 73%", text);
    TEST_ASSERT_TRUE(StatusText::wifi(text, sizeof(text), "Workshop", 150, true));
    TEST_ASSERT_EQUAL_STRING("WIFI Workshop 100%", text);
}

void test_provisioning_status_names_the_setup_ap() {
    char text[96]{};
    TEST_ASSERT_TRUE(StatusText::wifi(text, sizeof(text), "greenpico", 0, false));
    TEST_ASSERT_EQUAL_STRING("WIFI greenpico AP", text);
}

void test_ntp_status_contains_peer_and_sync_state() {
    char text[96]{};
    TEST_ASSERT_TRUE(StatusText::ntp(text, sizeof(text), "pool.ntp.org", true));
    TEST_ASSERT_EQUAL_STRING("NTP pool.ntp.org IN-SYNC", text);
    TEST_ASSERT_TRUE(StatusText::ntp(text, sizeof(text), "pool.ntp.org", false));
    TEST_ASSERT_EQUAL_STRING("NTP pool.ntp.org NOT-SYNCED", text);
}

void test_brightness_status_contains_sensor_output_and_mode() {
    char text[96]{};
    TEST_ASSERT_TRUE(StatusText::brightness(text, sizeof(text), 42, 60, true));
    TEST_ASSERT_EQUAL_STRING("LIGHT 42% BRIGHTNESS 60% AUTO", text);
    TEST_ASSERT_TRUE(StatusText::brightness(text, sizeof(text), 42, 70, false));
    TEST_ASSERT_EQUAL_STRING("LIGHT 42% BRIGHTNESS 70% MANUAL", text);
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_wifi_status_contains_ssid_and_bounded_strength);
    RUN_TEST(test_provisioning_status_names_the_setup_ap);
    RUN_TEST(test_ntp_status_contains_peer_and_sync_state);
    RUN_TEST(test_brightness_status_contains_sensor_output_and_mode);
    return UNITY_END();
}
