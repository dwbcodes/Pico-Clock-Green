#include "status_text.hpp"

#include <algorithm>
#include <cstdio>

namespace pico_clock {

bool StatusText::wifi(char* output, std::size_t size, const char* ssid,
                      int signal_percent, bool connected) {
    if (output == nullptr || size == 0) return false;
    const int bounded = std::max(0, std::min(100, signal_percent));
    const char* name = ssid != nullptr && ssid[0] != '\0' ? ssid : "greenpico";
    const int written = connected
        ? std::snprintf(output, size, "WIFI %s %d%%", name, bounded)
        : std::snprintf(output, size, "WIFI %s AP", name);
    return written >= 0 && static_cast<std::size_t>(written) < size;
}

bool StatusText::ntp(char* output, std::size_t size, const char* peer,
                     bool synchronized) {
    if (output == nullptr || size == 0) return false;
    const char* name = peer != nullptr && peer[0] != '\0' ? peer : "UNKNOWN";
    const int written = std::snprintf(output, size, "NTP %s %s", name,
                                      synchronized ? "IN-SYNC" : "NOT-SYNCED");
    return written >= 0 && static_cast<std::size_t>(written) < size;
}

bool StatusText::brightness(char* output, std::size_t size,
                            int ambient_light_percent,
                            int display_brightness_percent, bool automatic) {
    if (output == nullptr || size == 0) return false;
    const int light = std::max(0, std::min(100, ambient_light_percent));
    const int brightness = std::max(0, std::min(100,
                                                display_brightness_percent));
    const int written = std::snprintf(output, size,
        "LIGHT %d%% BRIGHTNESS %d%% %s", light, brightness,
        automatic ? "AUTO" : "MANUAL");
    return written >= 0 && static_cast<std::size_t>(written) < size;
}

}  // namespace pico_clock
