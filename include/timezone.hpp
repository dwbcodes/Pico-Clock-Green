#pragma once

#include <cstddef>
#include <cstdint>

namespace pico_clock {

struct LocalTime {
    std::int16_t year;
    std::int8_t month;
    std::int8_t day;
    std::int8_t weekday;
    std::int8_t hour;
    std::int8_t minute;
    std::int8_t second;
    std::int16_t utc_offset_minutes;
    bool daylight_saving;
};

class Timezone {
public:
    static constexpr std::size_t kNameSize = 32;

    static std::size_t count();
    static const char* name(std::size_t index);
    static bool supported(const char* name);
    static bool to_local(std::uint32_t unix_seconds, const char* name,
                         LocalTime& local);
};

}  // namespace pico_clock
