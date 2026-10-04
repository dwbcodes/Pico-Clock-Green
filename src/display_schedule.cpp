#include "display_schedule.hpp"

#include <cstdio>
#include <cstring>

namespace pico_clock {

bool DisplaySchedule::parse_time(const char* value, std::uint16_t& minute_of_day) {
    if (value == nullptr || std::strlen(value) != 5 || value[2] != ':' ||
        value[0] < '0' || value[0] > '9' || value[1] < '0' || value[1] > '9' ||
        value[3] < '0' || value[3] > '9' || value[4] < '0' || value[4] > '9') {
        return false;
    }
    const int hour = (value[0] - '0') * 10 + value[1] - '0';
    const int minute = (value[3] - '0') * 10 + value[4] - '0';
    if (hour > 23 || minute > 59) return false;
    minute_of_day = static_cast<std::uint16_t>(hour * 60 + minute);
    return true;
}

bool DisplaySchedule::format_time(std::uint16_t minute_of_day, char* output,
                                  std::size_t output_size) {
    if (minute_of_day >= 24 * 60 || output == nullptr || output_size < 6) return false;
    return std::snprintf(output, output_size, "%02u:%02u",
                         minute_of_day / 60, minute_of_day % 60) == 5;
}

bool DisplaySchedule::display_on(bool enabled, std::uint16_t off_minute,
                                 std::uint16_t on_minute, int hour, int minute) {
    if (!enabled) return true;
    if (off_minute >= 24 * 60 || on_minute >= 24 * 60 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        off_minute == on_minute) {
        return true;
    }
    const auto now = static_cast<std::uint16_t>(hour * 60 + minute);
    const bool off = off_minute < on_minute
        ? now >= off_minute && now < on_minute
        : now >= off_minute || now < on_minute;
    return !off;
}

}  // namespace pico_clock
