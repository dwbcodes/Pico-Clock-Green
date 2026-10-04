#pragma once

#include <cstdint>

namespace pico_clock {

struct PowerSaveStatus {
    bool active{false};
    bool test_active{false};
    std::uint8_t test_duration_seconds{0};
    std::uint8_t test_seconds_remaining{0};
};

class PowerSaveController {
public:
    static bool valid_test_duration(unsigned seconds);
    bool start_test(unsigned seconds, std::uint32_t now_ms);
    void update(bool normal_display_enabled, bool button_pressed,
                std::uint32_t now_ms);
    bool display_enabled(bool normal_display_enabled) const;
    PowerSaveStatus status(std::uint32_t now_ms) const;

private:
    bool test_active_{false};
    bool active_{false};
    std::uint8_t test_duration_seconds_{0};
    std::uint32_t test_deadline_ms_{0};
};

}  // namespace pico_clock
