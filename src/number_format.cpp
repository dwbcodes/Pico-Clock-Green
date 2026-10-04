#include "number_format.hpp"

#include <cstdio>

namespace pico_clock {

bool NumberFormat::fixed_two(float value, char* output,
                             std::size_t output_size) {
    if (output == nullptr || output_size == 0) return false;
    const long hundredths = value >= 0.0f
        ? static_cast<long>(value * 100.0f + 0.5f)
        : static_cast<long>(value * 100.0f - 0.5f);
    const bool negative = hundredths < 0;
    const unsigned long magnitude = static_cast<unsigned long>(
        negative ? -hundredths : hundredths);
    const int written = std::snprintf(
        output, output_size, "%s%lu.%02lu", negative ? "-" : "",
        magnitude / 100u, magnitude % 100u);
    return written > 0 && static_cast<std::size_t>(written) < output_size;
}

}  // namespace pico_clock
