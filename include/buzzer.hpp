#pragma once

#include <cstdint>

namespace pico_clock {

class Buzzer {
public:
    void init();
    void beep(std::uint32_t now_ms);
    void poll(std::uint32_t now_ms);

private:
    bool active_{false};
    std::uint32_t stop_at_ms_{0};
};

}  // namespace pico_clock
