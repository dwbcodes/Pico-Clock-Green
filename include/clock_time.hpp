#pragma once

#include <cstddef>
#include <cstdint>

namespace pico_clock {

struct ClockTime {
    bool valid{false};
    std::int16_t year{0};
    std::int8_t month{0};
    std::int8_t day{0};
    std::int8_t weekday{0};
    std::int8_t hour{0};
    std::int8_t minute{0};
    std::int8_t second{0};
};

bool format_clock_time_json(const ClockTime& time, const char* timezone,
                            bool ntp_synchronized, char* output,
                            std::size_t output_size);
bool format_clock_time_iso_json_value(const ClockTime& time, char* output,
                                      std::size_t output_size);
bool clock_time_difference_seconds(const ClockTime& left, const ClockTime& right,
                                   std::int32_t& difference);

}  // namespace pico_clock
