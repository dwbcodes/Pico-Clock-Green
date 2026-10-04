#include "config_json.hpp"

#include <cstring>
#include <cstdlib>

#include "display_schedule.hpp"
#include "chime_schedule.hpp"
#include "timezone.hpp"

namespace pico_clock {
namespace {

const char* skip_space(const char* cursor) {
    while (cursor != nullptr && (*cursor == ' ' || *cursor == '\t' ||
           *cursor == '\r' || *cursor == '\n')) {
        ++cursor;
    }
    return cursor;
}

bool copy_field(char* destination, std::size_t size, const char* value) {
    if (destination == nullptr || size == 0 || value == nullptr) return false;
    std::memset(destination, 0, size);
    std::strncpy(destination, value, size - 1);
    return true;
}

}  // namespace

const char* ConfigJson::find_value(const char* json, const char* key) {
    if (json == nullptr || key == nullptr || key[0] == '\0') return nullptr;
    const auto key_length = std::strlen(key);
    const char* cursor = json;
    while ((cursor = std::strchr(cursor, '"')) != nullptr) {
        ++cursor;
        const char* end = cursor;
        bool escaped = false;
        while (*end != '\0') {
            if (!escaped && *end == '"') break;
            escaped = !escaped && *end == '\\';
            if (*end != '\\') escaped = false;
            ++end;
        }
        if (*end == '\0') return nullptr;
        const char* after = skip_space(end + 1);
        if (static_cast<std::size_t>(end - cursor) == key_length &&
            std::strncmp(cursor, key, key_length) == 0 && *after == ':') {
            return skip_space(after + 1);
        }
        cursor = end + 1;
    }
    return nullptr;
}

bool ConfigJson::has_key(const char* json, const char* key) {
    return find_value(json, key) != nullptr;
}

bool ConfigJson::string_value(const char* json, const char* key, char* output,
                              std::size_t output_size) {
    const char* cursor = find_value(json, key);
    if (cursor == nullptr || *cursor++ != '"' || output == nullptr ||
        output_size == 0) {
        return false;
    }

    std::size_t written = 0;
    while (*cursor != '\0' && *cursor != '"') {
        char value = *cursor++;
        if (value == '\\') {
            const char escaped = *cursor++;
            switch (escaped) {
                case '"': value = '"'; break;
                case '\\': value = '\\'; break;
                case '/': value = '/'; break;
                case 'b': value = '\b'; break;
                case 'f': value = '\f'; break;
                case 'n': value = '\n'; break;
                case 'r': value = '\r'; break;
                case 't': value = '\t'; break;
                default: return false;
            }
        }
        if (static_cast<unsigned char>(value) < 0x20 || written + 1 >= output_size) {
            return false;
        }
        output[written++] = value;
    }
    if (*cursor != '"') return false;
    output[written] = '\0';
    return true;
}

bool ConfigJson::bool_value(const char* json, const char* key, bool& output) {
    const char* value = find_value(json, key);
    if (value == nullptr) return false;
    if (std::strncmp(value, "true", 4) == 0) {
        output = true;
        return true;
    }
    if (std::strncmp(value, "false", 5) == 0) {
        output = false;
        return true;
    }
    return false;
}

bool ConfigJson::unsigned_value(const char* json, const char* key,
                                unsigned& output) {
    const char* value = find_value(json, key);
    if (value == nullptr || *value < '0' || *value > '9') return false;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    end = const_cast<char*>(skip_space(end));
    if (end == value || (*end != ',' && *end != '}') || parsed > 1000) {
        return false;
    }
    output = static_cast<unsigned>(parsed);
    return true;
}

bool ConfigJson::escape(const char* input, char* output, std::size_t output_size) {
    if (input == nullptr || output == nullptr || output_size == 0) return false;
    std::size_t written = 0;
    while (*input != '\0') {
        const char* replacement = nullptr;
        switch (*input) {
            case '"': replacement = "\\\""; break;
            case '\\': replacement = "\\\\"; break;
            case '\b': replacement = "\\b"; break;
            case '\f': replacement = "\\f"; break;
            case '\n': replacement = "\\n"; break;
            case '\r': replacement = "\\r"; break;
            case '\t': replacement = "\\t"; break;
            default:
                if (static_cast<unsigned char>(*input) < 0x20) return false;
                break;
        }
        if (replacement != nullptr) {
            const auto length = std::strlen(replacement);
            if (written + length + 1 > output_size) return false;
            std::memcpy(output + written, replacement, length);
            written += length;
        } else {
            if (written + 2 > output_size) return false;
            output[written++] = *input;
        }
        ++input;
    }
    output[written] = '\0';
    return true;
}

bool ConfigJson::parse(const char* body, const AppConfig& current,
                       AppConfig& updated) {
    if (body == nullptr) return false;
    char ssid[AppConfig::kSsidSize]{};
    char ntp[AppConfig::kNtpServerSize]{};
    char timezone[Timezone::kNameSize]{};
    char off[6]{};
    char on[6]{};
    bool schedule_enabled = false;
    if (!string_value(body, "ssid", ssid, sizeof(ssid)) || ssid[0] == '\0' ||
        !string_value(body, "ntpServer", ntp, sizeof(ntp)) || ntp[0] == '\0' ||
        !string_value(body, "timezone", timezone, sizeof(timezone)) ||
        !Timezone::supported(timezone) ||
        !bool_value(body, "displayScheduleEnabled", schedule_enabled) ||
        !string_value(body, "displayOff", off, sizeof(off)) ||
        !string_value(body, "displayOn", on, sizeof(on))) {
        return false;
    }

    std::uint16_t off_minute = 0;
    std::uint16_t on_minute = 0;
    if (!DisplaySchedule::parse_time(off, off_minute) ||
        !DisplaySchedule::parse_time(on, on_minute) ||
        (schedule_enabled && off_minute == on_minute)) {
        return false;
    }

    updated = current;
    copy_field(updated.ssid, sizeof(updated.ssid), ssid);
    copy_field(updated.ntp_server, sizeof(updated.ntp_server), ntp);
    copy_field(updated.timezone, sizeof(updated.timezone), timezone);
    updated.display_schedule_enabled = schedule_enabled;
    updated.display_off_minute = off_minute;
    updated.display_on_minute = on_minute;

    if (has_key(body, "hostname")) {
        char hostname[AppConfig::kHostnameSize]{};
        if (!string_value(body, "hostname", hostname, sizeof(hostname)) ||
            !valid_hostname(hostname)) {
            return false;
        }
        copy_field(updated.hostname, sizeof(updated.hostname), hostname);
    }
    if (has_key(body, "domainName")) {
        char domain_name[AppConfig::kDomainNameSize]{};
        if (!string_value(body, "domainName", domain_name,
                          sizeof(domain_name)) ||
            !valid_domain_name(domain_name)) {
            return false;
        }
        copy_field(updated.domain_name, sizeof(updated.domain_name), domain_name);
    }

    if (has_key(body, "automaticBrightness") &&
        !bool_value(body, "automaticBrightness", updated.automatic_brightness)) {
        return false;
    }
    if (has_key(body, "manualBrightnessPercent")) {
        unsigned percent = 0;
        if (!unsigned_value(body, "manualBrightnessPercent", percent) ||
            percent < 10 || percent > 100 || (percent % 10) != 0) {
            return false;
        }
        updated.manual_brightness_percent = static_cast<std::uint8_t>(percent);
    }
    if (has_key(body, "chimeInterval")) {
        char interval[8]{};
        if (!string_value(body, "chimeInterval", interval, sizeof(interval)) ||
            !parse_chime_interval(interval, updated.chime_interval_minutes)) {
            return false;
        }
    }

    bool clear_password = false;
    if (has_key(body, "clearPassword")) {
        if (!bool_value(body, "clearPassword", clear_password)) return false;
    }
    if (clear_password) {
        std::memset(updated.password, 0, sizeof(updated.password));
    } else if (has_key(body, "password")) {
        char password[AppConfig::kPasswordSize]{};
        if (!string_value(body, "password", password, sizeof(password))) return false;
        copy_field(updated.password, sizeof(updated.password), password);
    }
    return updated.valid();
}

}  // namespace pico_clock
