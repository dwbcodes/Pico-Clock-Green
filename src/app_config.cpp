#include "app_config.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/flash.h"
#include "pico/platform.h"
#include "display_schedule.hpp"

#ifndef PICO_CLOCK_CONFIG_FLASH_OFFSET
#error "PICO_CLOCK_CONFIG_FLASH_OFFSET must reserve one flash sector"
#endif

namespace pico_clock {
namespace {

constexpr std::uint32_t kMagic = 0x50434347;  // "PCCG"
constexpr std::uint16_t kVersion = 5;

struct LegacyAppConfig {
    char ssid[AppConfig::kSsidSize];
    char password[AppConfig::kPasswordSize];
    char ntp_server[AppConfig::kNtpServerSize];
    std::int16_t utc_offset_minutes;
};

struct PersistentConfigV1 {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t size;
    LegacyAppConfig config;
    std::uint32_t checksum;
};

struct LegacyAppConfigV2 {
    char ssid[AppConfig::kSsidSize];
    char password[AppConfig::kPasswordSize];
    char ntp_server[AppConfig::kNtpServerSize];
    char timezone[Timezone::kNameSize];
    std::uint16_t display_off_minute;
    std::uint16_t display_on_minute;
    bool display_schedule_enabled;
};

struct PersistentConfigV2 {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t size;
    LegacyAppConfigV2 config;
    std::uint32_t checksum;
};

struct LegacyAppConfigV3 {
    char ssid[AppConfig::kSsidSize];
    char password[AppConfig::kPasswordSize];
    char ntp_server[AppConfig::kNtpServerSize];
    char timezone[Timezone::kNameSize];
    std::uint16_t display_off_minute;
    std::uint16_t display_on_minute;
    bool display_schedule_enabled;
    bool automatic_brightness;
    std::uint8_t manual_brightness_percent;
};

struct PersistentConfigV3 {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t size;
    LegacyAppConfigV3 config;
    std::uint32_t checksum;
};

struct LegacyAppConfigV4 {
    char ssid[AppConfig::kSsidSize];
    char password[AppConfig::kPasswordSize];
    char ntp_server[AppConfig::kNtpServerSize];
    char timezone[Timezone::kNameSize];
    std::uint16_t display_off_minute;
    std::uint16_t display_on_minute;
    bool display_schedule_enabled;
    bool automatic_brightness;
    std::uint8_t manual_brightness_percent;
    std::uint8_t chime_interval_minutes;
};

struct PersistentConfigV4 {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t size;
    LegacyAppConfigV4 config;
    std::uint32_t checksum;
};

struct PersistentConfigV5 {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t size;
    AppConfig config;
    std::uint32_t checksum;
};

const char* migrated_timezone(std::int16_t offset) {
    switch (offset) {
        case -480:
        case -420: return "America/Los_Angeles";
        case -360: return "America/Chicago";
        case -300: return "America/New_York";
        case 0: return "UTC";
        case 60:
        case 120: return "Europe/Berlin";
        case 540: return "Asia/Tokyo";
        default: return "UTC";
    }
}

struct FlashWrite {
    std::uint32_t offset;
    const std::uint8_t* data;
};

void write_flash(void* argument) {
    const auto* operation = static_cast<const FlashWrite*>(argument);
    flash_range_erase(operation->offset, FLASH_SECTOR_SIZE);
    flash_range_program(operation->offset, operation->data, FLASH_SECTOR_SIZE);
}

void erase_flash(void* argument) {
    const auto offset = *static_cast<const std::uint32_t*>(argument);
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
}

}  // namespace

std::uint32_t ConfigStore::checksum(const std::uint8_t* data, std::size_t size) {
    std::uint32_t value = 2166136261u;
    for (std::size_t index = 0; index < size; ++index) {
        value ^= data[index];
        value *= 16777619u;
    }
    return value;
}

bool ConfigStore::load(AppConfig& config) const {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        XIP_BASE + static_cast<std::uint32_t>(PICO_CLOCK_CONFIG_FLASH_OFFSET));
    const auto* header = reinterpret_cast<const PersistentConfigV5*>(bytes);
    if (header->magic != kMagic) return false;

    if (header->version == kVersion && header->size == sizeof(PersistentConfigV5)) {
        const auto* v5 = reinterpret_cast<const PersistentConfigV5*>(bytes);
        const auto expected = checksum(bytes, offsetof(PersistentConfigV5, checksum));
        if (expected != v5->checksum) return false;
        config = v5->config;
    } else if (header->version == 4 && header->size == sizeof(PersistentConfigV4)) {
        const auto* v4 = reinterpret_cast<const PersistentConfigV4*>(bytes);
        const auto expected = checksum(bytes, offsetof(PersistentConfigV4, checksum));
        if (expected != v4->checksum) return false;
        config = AppConfig{};
        std::memcpy(config.ssid, v4->config.ssid, sizeof(config.ssid));
        std::memcpy(config.password, v4->config.password, sizeof(config.password));
        std::memcpy(config.ntp_server, v4->config.ntp_server,
                    sizeof(config.ntp_server));
        std::memcpy(config.timezone, v4->config.timezone,
                    sizeof(config.timezone));
        config.display_off_minute = v4->config.display_off_minute;
        config.display_on_minute = v4->config.display_on_minute;
        config.display_schedule_enabled = v4->config.display_schedule_enabled;
        config.automatic_brightness = v4->config.automatic_brightness;
        config.manual_brightness_percent = v4->config.manual_brightness_percent;
        config.chime_interval_minutes = v4->config.chime_interval_minutes;
    } else if (header->version == 3 && header->size == sizeof(PersistentConfigV3)) {
        const auto* v3 = reinterpret_cast<const PersistentConfigV3*>(bytes);
        const auto expected = checksum(bytes, offsetof(PersistentConfigV3, checksum));
        if (expected != v3->checksum) return false;
        config = AppConfig{};
        std::memcpy(config.ssid, v3->config.ssid, sizeof(config.ssid));
        std::memcpy(config.password, v3->config.password, sizeof(config.password));
        std::memcpy(config.ntp_server, v3->config.ntp_server,
                    sizeof(config.ntp_server));
        std::memcpy(config.timezone, v3->config.timezone,
                    sizeof(config.timezone));
        config.display_off_minute = v3->config.display_off_minute;
        config.display_on_minute = v3->config.display_on_minute;
        config.display_schedule_enabled = v3->config.display_schedule_enabled;
        config.automatic_brightness = v3->config.automatic_brightness;
        config.manual_brightness_percent = v3->config.manual_brightness_percent;
    } else if (header->version == 2 && header->size == sizeof(PersistentConfigV2)) {
        const auto* v2 = reinterpret_cast<const PersistentConfigV2*>(bytes);
        const auto expected = checksum(bytes, offsetof(PersistentConfigV2, checksum));
        if (expected != v2->checksum) return false;
        config = AppConfig{};
        std::memcpy(config.ssid, v2->config.ssid, sizeof(config.ssid));
        std::memcpy(config.password, v2->config.password, sizeof(config.password));
        std::memcpy(config.ntp_server, v2->config.ntp_server,
                    sizeof(config.ntp_server));
        std::memcpy(config.timezone, v2->config.timezone, sizeof(config.timezone));
        config.display_off_minute = v2->config.display_off_minute;
        config.display_on_minute = v2->config.display_on_minute;
        config.display_schedule_enabled = v2->config.display_schedule_enabled;
    } else if (header->version == 1 && header->size == sizeof(PersistentConfigV1)) {
        const auto* v1 = reinterpret_cast<const PersistentConfigV1*>(bytes);
        const auto expected = checksum(bytes, offsetof(PersistentConfigV1, checksum));
        if (expected != v1->checksum) return false;
        config = AppConfig{};
        std::memcpy(config.ssid, v1->config.ssid, sizeof(config.ssid));
        std::memcpy(config.password, v1->config.password, sizeof(config.password));
        std::memcpy(config.ntp_server, v1->config.ntp_server,
                    sizeof(config.ntp_server));
        std::snprintf(config.timezone, sizeof(config.timezone), "%s",
                      migrated_timezone(v1->config.utc_offset_minutes));
    } else {
        return false;
    }

    config.ssid[AppConfig::kSsidSize - 1] = '\0';
    config.password[AppConfig::kPasswordSize - 1] = '\0';
    config.hostname[AppConfig::kHostnameSize - 1] = '\0';
    config.domain_name[AppConfig::kDomainNameSize - 1] = '\0';
    config.ntp_server[AppConfig::kNtpServerSize - 1] = '\0';
    config.timezone[Timezone::kNameSize - 1] = '\0';
    return config.valid();
}

bool ConfigStore::save(const AppConfig& config) const {
    static_assert((PICO_CLOCK_CONFIG_FLASH_OFFSET % FLASH_SECTOR_SIZE) == 0,
                  "Configuration flash offset must be sector aligned");

    // A flash sector is larger than the Pico's normal main-thread stack.
    // Static scratch storage avoids a stack overflow during configuration save.
    alignas(FLASH_PAGE_SIZE) static std::uint8_t sector[FLASH_SECTOR_SIZE];
    std::memset(sector, 0xff, sizeof(sector));

    PersistentConfigV5 stored{};
    stored.magic = kMagic;
    stored.version = kVersion;
    stored.size = sizeof(stored);
    stored.config = config;
    stored.checksum = checksum(reinterpret_cast<const std::uint8_t*>(&stored),
                               offsetof(PersistentConfigV5, checksum));
    std::memcpy(sector, &stored, sizeof(stored));

    FlashWrite operation{
        static_cast<std::uint32_t>(PICO_CLOCK_CONFIG_FLASH_OFFSET), sector};
    return flash_safe_execute(write_flash, &operation, UINT32_MAX) == PICO_OK;
}

bool ConfigStore::clear() const {
    std::uint32_t offset =
        static_cast<std::uint32_t>(PICO_CLOCK_CONFIG_FLASH_OFFSET);
    return flash_safe_execute(erase_flash, &offset, UINT32_MAX) == PICO_OK;
}

}  // namespace pico_clock
