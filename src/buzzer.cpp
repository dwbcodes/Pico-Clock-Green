#include "buzzer.hpp"

#include "board.hpp"
#include "hardware/gpio.h"

namespace pico_clock {
namespace {

constexpr std::uint32_t kBeepDurationMs = 120;

}  // namespace

void Buzzer::init() {
    gpio_init(board::kBuzzer);
    gpio_set_dir(board::kBuzzer, GPIO_OUT);
    gpio_put(board::kBuzzer, false);
}

void Buzzer::beep(std::uint32_t now_ms) {
    gpio_put(board::kBuzzer, true);
    active_ = true;
    stop_at_ms_ = now_ms + kBeepDurationMs;
}

void Buzzer::poll(std::uint32_t now_ms) {
    if (active_ && static_cast<std::int32_t>(now_ms - stop_at_ms_) >= 0) {
        gpio_put(board::kBuzzer, false);
        active_ = false;
    }
}

}  // namespace pico_clock
