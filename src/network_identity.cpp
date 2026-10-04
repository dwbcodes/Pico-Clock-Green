#include "network_identity.hpp"

#include <cstdio>
#include <cstring>

namespace pico_clock {
namespace {

bool valid_label(const char* value, std::size_t length) {
    if (value == nullptr || length == 0 || length > 63) return false;
    const auto alphanumeric = [](char character) {
        return (character >= 'a' && character <= 'z') ||
               (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9');
    };
    if (!alphanumeric(value[0]) || !alphanumeric(value[length - 1])) return false;
    for (std::size_t index = 1; index + 1 < length; ++index) {
        if (!alphanumeric(value[index]) && value[index] != '-') return false;
    }
    return true;
}

}  // namespace

bool valid_hostname(const char* value) {
    return value != nullptr && valid_label(value, std::strlen(value));
}

bool valid_domain_name(const char* value) {
    if (value == nullptr) return false;
    const std::size_t total = std::strlen(value);
    if (total == 0) return true;
    if (total > 253 || value[0] == '.' || value[total - 1] == '.') return false;
    const char* label = value;
    while (label < value + total) {
        const char* dot = std::strchr(label, '.');
        const std::size_t length = dot == nullptr
            ? static_cast<std::size_t>(value + total - label)
            : static_cast<std::size_t>(dot - label);
        if (!valid_label(label, length)) return false;
        if (dot == nullptr) break;
        label = dot + 1;
    }
    return true;
}

bool valid_network_identity(const char* hostname, const char* domain_name) {
    if (!valid_hostname(hostname) || !valid_domain_name(domain_name)) return false;
    const std::size_t hostname_length = std::strlen(hostname);
    const std::size_t domain_length = std::strlen(domain_name);
    return domain_length == 0 || hostname_length + 1 + domain_length <= 253;
}

bool format_fqdn(const char* hostname, const char* domain_name, char* output,
                 std::size_t output_size) {
    if (!valid_network_identity(hostname, domain_name) ||
        output == nullptr || output_size == 0) {
        return false;
    }
    const int written = domain_name[0] == '\0'
        ? std::snprintf(output, output_size, "%s", hostname)
        : std::snprintf(output, output_size, "%s.%s", hostname, domain_name);
    return written >= 0 && static_cast<std::size_t>(written) < output_size;
}

}  // namespace pico_clock
