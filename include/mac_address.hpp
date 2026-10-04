#pragma once

#include <cstddef>
#include <cstdint>

namespace pico_clock {

constexpr std::size_t kMacAddressSize = 6;
constexpr std::size_t kFormattedMacAddressSize = 18;

bool is_valid_unicast_mac(const std::uint8_t mac[kMacAddressSize]);
void format_mac_address(const std::uint8_t mac[kMacAddressSize],
                        char output[kFormattedMacAddressSize]);

}  // namespace pico_clock
