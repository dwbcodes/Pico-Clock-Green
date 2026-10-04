#pragma once

#include <cstddef>
#include <cstdint>

namespace pico_clock {

class DisplaySchedule {
public:
    static bool parse_time(const char* value, std::uint16_t& minute_of_day);
    static bool format_time(std::uint16_t minute_of_day, char* output,
                            std::size_t output_size);
    static bool display_on(bool enabled, std::uint16_t off_minute,
                           std::uint16_t on_minute, int hour, int minute);
};

}  // namespace pico_clock
