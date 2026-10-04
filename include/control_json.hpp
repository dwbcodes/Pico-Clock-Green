#pragma once

#include "clock_control.hpp"

namespace pico_clock {

class ControlJson {
public:
    static bool parse_display_action(const char* body, ControlAction& action);
    static bool parse_power_save_test(const char* body,
                                      unsigned& duration_seconds);
    static bool confirms_factory_reset(const char* body);
};

}  // namespace pico_clock
