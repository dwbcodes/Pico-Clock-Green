#pragma once

#include <cstddef>

namespace pico_clock {

bool valid_hostname(const char* value);
bool valid_domain_name(const char* value);
bool valid_network_identity(const char* hostname, const char* domain_name);
bool format_fqdn(const char* hostname, const char* domain_name, char* output,
                 std::size_t output_size);

}  // namespace pico_clock
