#include "clock_time.hpp"

#include <cstdio>
#include <limits>

namespace pico_clock {
namespace {

bool leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int days_in_month(int year, int month) {
    constexpr int kDays[] = {31, 28, 31, 30, 31, 30,
                             31, 31, 30, 31, 30, 31};
    return month == 2 && leap_year(year) ? 29 : kDays[month - 1];
}

bool valid(const ClockTime& time) {
    return time.valid && time.year >= 1970 && time.month >= 1 && time.month <= 12 &&
           time.day >= 1 && time.day <= days_in_month(time.year, time.month) &&
           time.hour >= 0 && time.hour <= 23 && time.minute >= 0 &&
           time.minute <= 59 && time.second >= 0 && time.second <= 59;
}

std::int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned adjusted_month = month > 2 ? month - 3 : month + 9;
    const unsigned day_of_year = (153 * adjusted_month + 2) / 5 + day - 1;
    const unsigned day_of_era = year_of_era * 365 + year_of_era / 4 -
        year_of_era / 100 + day_of_year;
    return static_cast<std::int64_t>(era) * 146097 + day_of_era - 719468;
}

std::int64_t civil_seconds(const ClockTime& time) {
    return ((days_from_civil(time.year, static_cast<unsigned>(time.month),
                             static_cast<unsigned>(time.day)) * 24 + time.hour) *
            60 + time.minute) * 60 + time.second;
}

}  // namespace

bool format_clock_time_json(const ClockTime& time, const char* timezone,
                            bool ntp_synchronized, char* output,
                            std::size_t output_size) {
    if (output == nullptr || output_size == 0 || timezone == nullptr) return false;
    const int written = time.valid
        ? std::snprintf(output, output_size,
              "{\"valid\":true,\"date\":\"%04d-%02d-%02d\","
              "\"time\":\"%02d:%02d:%02d\",\"weekday\":%d,"
              "\"timezone\":\"%s\",\"ntpSynchronized\":%s}\n",
              time.year, time.month, time.day, time.hour, time.minute,
              time.second, time.weekday, timezone,
              ntp_synchronized ? "true" : "false")
        : std::snprintf(output, output_size,
              "{\"valid\":false,\"date\":null,\"time\":null,"
              "\"weekday\":null,\"timezone\":\"%s\","
              "\"ntpSynchronized\":%s}\n",
              timezone, ntp_synchronized ? "true" : "false");
    return written >= 0 && static_cast<std::size_t>(written) < output_size;
}

bool format_clock_time_iso_json_value(const ClockTime& time, char* output,
                                      std::size_t output_size) {
    if (output == nullptr || output_size == 0) return false;
    const int written = valid(time)
        ? std::snprintf(output, output_size,
              "\"%04d-%02d-%02dT%02d:%02d:%02d\"",
              time.year, time.month, time.day, time.hour, time.minute,
              time.second)
        : std::snprintf(output, output_size, "null");
    return written >= 0 && static_cast<std::size_t>(written) < output_size;
}

bool clock_time_difference_seconds(const ClockTime& left, const ClockTime& right,
                                   std::int32_t& difference) {
    if (!valid(left) || !valid(right)) return false;
    const auto value = civil_seconds(left) - civil_seconds(right);
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) return false;
    difference = static_cast<std::int32_t>(value);
    return true;
}

}  // namespace pico_clock
