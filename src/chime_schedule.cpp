#include "chime_schedule.hpp"

#include <cstring>

namespace pico_clock {

bool valid_chime_interval(std::uint8_t minutes) {
    return minutes == 0 || minutes == 15 || minutes == 30 || minutes == 60;
}

const char* chime_interval_name(std::uint8_t minutes) {
    switch (minutes) {
        case 15: return "15min";
        case 30: return "30min";
        case 60: return "1hr";
        default: return "never";
    }
}

bool parse_chime_interval(const char* name, std::uint8_t& minutes) {
    if (name == nullptr) return false;
    if (std::strcmp(name, "never") == 0) minutes = 0;
    else if (std::strcmp(name, "15min") == 0) minutes = 15;
    else if (std::strcmp(name, "30min") == 0) minutes = 30;
    else if (std::strcmp(name, "1hr") == 0) minutes = 60;
    else return false;
    return true;
}

bool chime_due(std::uint8_t interval_minutes, int minute, int second) {
    return valid_chime_interval(interval_minutes) && interval_minutes != 0 &&
           minute >= 0 && minute < 60 && second == 0 &&
           (minute % interval_minutes) == 0;
}

}  // namespace pico_clock
