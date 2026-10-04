#pragma once

#include <cstdint>

namespace pico_clock {

bool valid_chime_interval(std::uint8_t minutes);
const char* chime_interval_name(std::uint8_t minutes);
bool parse_chime_interval(const char* name, std::uint8_t& minutes);
bool chime_due(std::uint8_t interval_minutes, int minute, int second);

}  // namespace pico_clock
