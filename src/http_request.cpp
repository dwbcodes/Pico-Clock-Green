#include "http_request.hpp"

#include <cstring>

namespace pico_clock {

bool HttpRequest::matches(const char* request, const char* method,
                          const char* path) {
    if (request == nullptr || method == nullptr || path == nullptr ||
        method[0] == '\0' || path[0] != '/') {
        return false;
    }
    const auto method_length = std::strlen(method);
    const auto path_length = std::strlen(path);
    return std::strncmp(request, method, method_length) == 0 &&
        request[method_length] == ' ' &&
        std::strncmp(request + method_length + 1, path, path_length) == 0 &&
        request[method_length + path_length + 1] == ' ';
}

}  // namespace pico_clock
