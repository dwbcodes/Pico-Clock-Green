#include <unity.h>

#include "wifi_scan.hpp"

using pico_clock::wifi_signal_percent;

namespace {

void test_signal_percent_clamps_and_scales_rssi() {
    TEST_ASSERT_EQUAL_INT(0, wifi_signal_percent(-100));
    TEST_ASSERT_EQUAL_INT(0, wifi_signal_percent(-90));
    TEST_ASSERT_EQUAL_INT(50, wifi_signal_percent(-65));
    TEST_ASSERT_EQUAL_INT(100, wifi_signal_percent(-40));
    TEST_ASSERT_EQUAL_INT(100, wifi_signal_percent(-20));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_signal_percent_clamps_and_scales_rssi);
    return UNITY_END();
}
