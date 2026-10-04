#include <cstdint>

#include <unity.h>

#include "mac_address.hpp"

using pico_clock::format_mac_address;
using pico_clock::is_valid_unicast_mac;

void setUp() {}
void tearDown() {}

void test_rejects_all_zero_address() {
    const std::uint8_t mac[6]{};
    TEST_ASSERT_FALSE(is_valid_unicast_mac(mac));
}

void test_rejects_broadcast_and_multicast_addresses() {
    const std::uint8_t broadcast[6]{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    const std::uint8_t multicast[6]{0x01, 0x00, 0x5e, 0x00, 0x00, 0x01};
    TEST_ASSERT_FALSE(is_valid_unicast_mac(broadcast));
    TEST_ASSERT_FALSE(is_valid_unicast_mac(multicast));
}

void test_accepts_universal_and_locally_administered_addresses() {
    const std::uint8_t universal[6]{0x28, 0xcd, 0xc1, 0x12, 0x34, 0x56};
    const std::uint8_t local[6]{0x2a, 0xcd, 0xc1, 0x12, 0x34, 0x56};
    TEST_ASSERT_TRUE(is_valid_unicast_mac(universal));
    TEST_ASSERT_TRUE(is_valid_unicast_mac(local));
}

void test_formats_address_for_display() {
    const std::uint8_t mac[6]{0x2a, 0xcd, 0x01, 0x02, 0xab, 0xef};
    char output[18]{};
    format_mac_address(mac, output);
    TEST_ASSERT_EQUAL_STRING("2A:CD:01:02:AB:EF", output);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rejects_all_zero_address);
    RUN_TEST(test_rejects_broadcast_and_multicast_addresses);
    RUN_TEST(test_accepts_universal_and_locally_administered_addresses);
    RUN_TEST(test_formats_address_for_display);
    return UNITY_END();
}
