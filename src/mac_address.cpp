#include "mac_address.hpp"

#include <cstdio>

namespace pico_clock {

bool is_valid_unicast_mac(const std::uint8_t mac[kMacAddressSize]) {
    bool all_zero = true;
    bool all_ones = true;
    for (std::size_t index = 0; index < kMacAddressSize; ++index) {
        all_zero = all_zero && mac[index] == 0x00;
        all_ones = all_ones && mac[index] == 0xff;
    }
    return !all_zero && !all_ones && (mac[0] & 0x01) == 0;
}

void format_mac_address(const std::uint8_t mac[kMacAddressSize],
                        char output[kFormattedMacAddressSize]) {
    std::snprintf(output, kFormattedMacAddressSize,
                  "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

}  // namespace pico_clock
