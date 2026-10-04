#include "http_headers.hpp"

#include <cstdio>

namespace pico_clock {

int HttpHeaders::format(char* output, std::size_t output_size,
                        const char* status, const char* content_type,
                        std::size_t content_length, bool gzip,
                        const char* cache_control) {
    // Pico's size-optimized printf does not implement the C99 %zu modifier.
    // size_t is 32-bit on RP2040, so promote it explicitly and use %lu.
    const auto length = static_cast<unsigned long>(content_length);
    if (gzip) {
        return std::snprintf(output, output_size,
            "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Encoding: gzip\r\n"
            "Content-Length: %lu\r\nConnection: close\r\n"
            "Cache-Control: %s\r\n\r\n",
            status, content_type, length, cache_control);
    }
    return std::snprintf(output, output_size,
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\n"
        "Connection: close\r\nCache-Control: %s\r\n\r\n",
        status, content_type, length, cache_control);
}

}  // namespace pico_clock
