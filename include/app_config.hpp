#pragma once

#include <cstddef>
#include <cstdint>

#include "timezone.hpp"
#include "chime_schedule.hpp"
#include "network_identity.hpp"

namespace pico_clock {

struct AppConfig {
    static constexpr std::size_t kSsidSize = 33;
    static constexpr std::size_t kPasswordSize = 64;
    static constexpr std::size_t kNtpServerSize = 64;
    static constexpr std::size_t kHostnameSize = 64;
    static constexpr std::size_t kDomainNameSize = 254;
    static constexpr std::size_t kFqdnSize = kHostnameSize + kDomainNameSize;

    char ssid[kSsidSize]{};
    char password[kPasswordSize]{};
    char hostname[kHostnameSize]{"greenpico"};
    char domain_name[kDomainNameSize]{"internal"};
    char ntp_server[kNtpServerSize]{"pool.ntp.org"};
    char timezone[Timezone::kNameSize]{"America/Los_Angeles"};
    std::uint16_t display_off_minute{22 * 60};
    std::uint16_t display_on_minute{7 * 60};
    bool display_schedule_enabled{false};
    bool automatic_brightness{true};
    std::uint8_t manual_brightness_percent{50};
    std::uint8_t chime_interval_minutes{0};

    bool has_wifi() const { return ssid[0] != '\0'; }
    bool valid() const {
        return valid_network_identity(hostname, domain_name) &&
               ntp_server[0] != '\0' && Timezone::supported(timezone) &&
               display_off_minute < 24 * 60 && display_on_minute < 24 * 60 &&
               manual_brightness_percent >= 10 &&
               manual_brightness_percent <= 100 &&
               valid_chime_interval(chime_interval_minutes) &&
               (!display_schedule_enabled || display_off_minute != display_on_minute);
    }
};

class ConfigStore {
public:
    bool load(AppConfig& config) const;
    bool save(const AppConfig& config) const;
    bool clear() const;

private:
    static std::uint32_t checksum(const std::uint8_t* data, std::size_t size);
};

}  // namespace pico_clock
