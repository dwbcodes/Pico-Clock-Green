#pragma once

namespace pico_clock {

class HttpRequest {
public:
    static bool matches(const char* request, const char* method,
                        const char* path);
};

}  // namespace pico_clock
