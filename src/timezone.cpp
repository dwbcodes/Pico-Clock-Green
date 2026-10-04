#include "timezone.hpp"

#include <array>
#include <cstring>
#include <ctime>

namespace pico_clock {
namespace {

enum class DstRule { None, UnitedStates, Europe };

struct Zone {
    const char* name;
    std::int16_t standard_offset;
    std::int16_t daylight_offset;
    DstRule rule;
};

constexpr std::array<Zone, 9> kZones{{
    {"UTC", 0, 0, DstRule::None},
    {"America/Los_Angeles", -480, -420, DstRule::UnitedStates},
    {"America/Denver", -420, -360, DstRule::UnitedStates},
    {"America/Chicago", -360, -300, DstRule::UnitedStates},
    {"America/New_York", -300, -240, DstRule::UnitedStates},
    {"America/Phoenix", -420, -420, DstRule::None},
    {"Europe/London", 0, 60, DstRule::Europe},
    {"Europe/Berlin", 60, 120, DstRule::Europe},
    {"Asia/Tokyo", 540, 540, DstRule::None},
}};

bool leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int days_in_month(int year, int month) {
    constexpr int kDays[] = {31, 28, 31, 30, 31, 30,
                             31, 31, 30, 31, 30, 31};
    return month == 2 && leap_year(year) ? 29 : kDays[month - 1];
}

std::int64_t civil_epoch(int year, int month, int day, int hour) {
    std::int64_t days = 0;
    for (int current = 1970; current < year; ++current) {
        days += leap_year(current) ? 366 : 365;
    }
    for (int current = 1; current < month; ++current) {
        days += days_in_month(year, current);
    }
    days += day - 1;
    return ((days * 24) + hour) * 60 * 60;
}

int weekday(int year, int month, int day) {
    const auto days = civil_epoch(year, month, day, 0) / (24 * 60 * 60);
    return static_cast<int>((days + 4) % 7);  // 1970-01-01 was Thursday.
}

int nth_sunday(int year, int month, int occurrence) {
    const int first_weekday = weekday(year, month, 1);
    return 1 + ((7 - first_weekday) % 7) + ((occurrence - 1) * 7);
}

int last_sunday(int year, int month) {
    const int last = days_in_month(year, month);
    return last - weekday(year, month, last);
}

const Zone* find_zone(const char* name) {
    if (name == nullptr) return nullptr;
    for (const auto& zone : kZones) {
        if (std::strcmp(name, zone.name) == 0) return &zone;
    }
    return nullptr;
}

bool daylight_saving(const Zone& zone, std::uint32_t epoch, int year) {
    if (zone.rule == DstRule::None) return false;

    std::int64_t start = 0;
    std::int64_t end = 0;
    if (zone.rule == DstRule::UnitedStates) {
        const int march_day = nth_sunday(year, 3, 2);
        const int november_day = nth_sunday(year, 11, 1);
        start = civil_epoch(year, 3, march_day, 2) -
                static_cast<std::int64_t>(zone.standard_offset) * 60;
        end = civil_epoch(year, 11, november_day, 2) -
              static_cast<std::int64_t>(zone.daylight_offset) * 60;
    } else {
        start = civil_epoch(year, 3, last_sunday(year, 3), 1);
        end = civil_epoch(year, 10, last_sunday(year, 10), 1);
    }
    return static_cast<std::int64_t>(epoch) >= start &&
           static_cast<std::int64_t>(epoch) < end;
}

}  // namespace

std::size_t Timezone::count() {
    return kZones.size();
}

const char* Timezone::name(std::size_t index) {
    return index < kZones.size() ? kZones[index].name : nullptr;
}

bool Timezone::supported(const char* name) {
    return find_zone(name) != nullptr;
}

bool Timezone::to_local(std::uint32_t unix_seconds, const char* name,
                        LocalTime& local) {
    const auto* zone = find_zone(name);
    if (zone == nullptr) return false;

    const std::time_t utc_epoch = static_cast<std::time_t>(unix_seconds);
    std::tm utc{};
    if (gmtime_r(&utc_epoch, &utc) == nullptr) return false;

    const bool dst = daylight_saving(*zone, unix_seconds, utc.tm_year + 1900);
    const std::int16_t offset = dst ? zone->daylight_offset : zone->standard_offset;
    const std::time_t local_epoch = utc_epoch + static_cast<std::time_t>(offset) * 60;
    std::tm converted{};
    if (gmtime_r(&local_epoch, &converted) == nullptr) return false;

    local = LocalTime{
        static_cast<std::int16_t>(converted.tm_year + 1900),
        static_cast<std::int8_t>(converted.tm_mon + 1),
        static_cast<std::int8_t>(converted.tm_mday),
        static_cast<std::int8_t>(converted.tm_wday),
        static_cast<std::int8_t>(converted.tm_hour),
        static_cast<std::int8_t>(converted.tm_min),
        static_cast<std::int8_t>(converted.tm_sec),
        offset,
        dst,
    };
    return true;
}

}  // namespace pico_clock
