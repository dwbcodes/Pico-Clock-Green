#pragma once

#include <cstddef>

namespace pico_clock {

class NumberFormat {
public:
    static bool fixed_two(float value, char* output, std::size_t output_size);
};

}  // namespace pico_clock
