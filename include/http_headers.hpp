#pragma once

#include <cstddef>

namespace pico_clock {

class HttpHeaders {
public:
    static int format(char* output, std::size_t output_size,
                      const char* status, const char* content_type,
                      std::size_t content_length, bool gzip,
                      const char* cache_control);
};

}  // namespace pico_clock
