#include <unity.h>

#include "wifi_policy.hpp"

using pico_clock::StartupNetworkMode;
using pico_clock::kDeviceName;
using pico_clock::startup_network_mode;

namespace {

void test_successful_saved_network_uses_station_only() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StartupNetworkMode::Station),
        static_cast<int>(startup_network_mode(true, true)));
}

void test_missing_or_failed_network_uses_provisioning() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StartupNetworkMode::Provisioning),
        static_cast<int>(startup_network_mode(false, false)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StartupNetworkMode::Provisioning),
        static_cast<int>(startup_network_mode(true, false)));
}

void test_device_name_is_greenpico() {
    TEST_ASSERT_EQUAL_STRING("greenpico", kDeviceName);
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_successful_saved_network_uses_station_only);
    RUN_TEST(test_missing_or_failed_network_uses_provisioning);
    RUN_TEST(test_device_name_is_greenpico);
    return UNITY_END();
}
