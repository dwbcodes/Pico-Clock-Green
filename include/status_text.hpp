#pragma once

#include <cstddef>

namespace pico_clock {

class StatusText {
public:
    static bool wifi(char* output, std::size_t size, const char* ssid,
                     int signal_percent, bool connected);
    static bool ntp(char* output, std::size_t size, const char* peer,
                    bool synchronized);
    static bool brightness(char* output, std::size_t size,
                           int ambient_light_percent,
                           int display_brightness_percent, bool automatic);
};

}  // namespace pico_clock
