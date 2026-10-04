#pragma once

#include <cstddef>

#include "app_config.hpp"

namespace pico_clock {

class ConfigJson {
public:
    static bool parse(const char* body, const AppConfig& current,
                      AppConfig& updated);
    static bool string_value(const char* json, const char* key, char* output,
                             std::size_t output_size);
    static bool bool_value(const char* json, const char* key, bool& output);
    static bool unsigned_value(const char* json, const char* key,
                               unsigned& output);
    static bool has_key(const char* json, const char* key);
    static bool escape(const char* input, char* output, std::size_t output_size);

private:
    static const char* find_value(const char* json, const char* key);
};

}  // namespace pico_clock
