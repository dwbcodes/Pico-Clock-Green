#include "led_display.hpp"

#include "board.hpp"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/platform.h"

namespace pico_clock {
namespace {

constexpr std::int64_t kRowPeriodUs = 1000;  // 125 complete frames per second.
constexpr std::uint16_t kBrightnessPwmWrap = 999;

void init_output(unsigned pin, bool initial_value = false) {
    gpio_init(pin);
    gpio_put(pin, initial_value);
    gpio_set_dir(pin, GPIO_OUT);
}

}  // namespace

bool LedDisplay::init() {
    // Disable output before changing the shift-register and row-address pins.
    init_output(board::kDisplayOutputEnable, true);
    init_output(board::kDisplayClock);
    init_output(board::kDisplayData);
    init_output(board::kDisplayLatch);
    init_output(board::kDisplayAddress0);
    init_output(board::kDisplayAddress1);
    init_output(board::kDisplayAddress2);

    // OE is active low. Hardware PWM provides flicker-free brightness without
    // dropping complete display frames. The GPIO output override still lets
    // the row scanner force OE high while shift-register data is changing.
    gpio_set_function(board::kDisplayOutputEnable, GPIO_FUNC_PWM);
    const unsigned pwm_slice = pwm_gpio_to_slice_num(board::kDisplayOutputEnable);
    pwm_config pwm = pwm_get_default_config();
    pwm_config_set_clkdiv(&pwm, 12.5f);  // About 10.6 kHz at 133 MHz.
    pwm_config_set_wrap(&pwm, kBrightnessPwmWrap);
    pwm_init(pwm_slice, &pwm, true);
    pwm_set_gpio_level(board::kDisplayOutputEnable, 0);  // Full brightness.
    gpio_set_outover(board::kDisplayOutputEnable, GPIO_OVERRIDE_HIGH);

    row_ = 0;
    active_buffer_ = 0;
    return add_repeating_timer_us(-kRowPeriodUs, scan_callback, this, &timer_);
}

void LedDisplay::set_frame(const ClockFace::Frame& frame) {
    const unsigned next = active_buffer_ ^ 1u;
    buffers_[next] = frame;
    __dmb();
    active_buffer_ = next;
}

void LedDisplay::set_enabled(bool enabled) {
    enabled_ = enabled;
}

void LedDisplay::set_brightness(std::uint8_t brightness) {
    brightness_ = brightness > 10 ? 10 : brightness;
    const std::uint16_t off_duty = static_cast<std::uint16_t>(
        (10u - brightness_) * ((kBrightnessPwmWrap + 1u) / 10u));
    pwm_set_gpio_level(board::kDisplayOutputEnable, off_duty);
}

bool LedDisplay::scan_callback(repeating_timer_t* timer) {
    static_cast<LedDisplay*>(timer->user_data)->scan_row();
    return true;
}

void LedDisplay::scan_row() {
    // OE is active low. Blanking while shifting prevents visible ghost pixels.
    gpio_set_outover(board::kDisplayOutputEnable, GPIO_OVERRIDE_HIGH);

    const std::uint32_t pixels = buffers_[active_buffer_][row_];
    for (int bit = 31; bit >= 0; --bit) {
        gpio_put(board::kDisplayClock, false);
        gpio_put(board::kDisplayData, (pixels & (std::uint32_t{1} << bit)) != 0);
        gpio_put(board::kDisplayClock, true);
    }
    gpio_put(board::kDisplayClock, false);

    gpio_put(board::kDisplayLatch, true);
    gpio_put(board::kDisplayLatch, false);

    gpio_put(board::kDisplayAddress0, (row_ & 0x01u) != 0);
    gpio_put(board::kDisplayAddress1, (row_ & 0x02u) != 0);
    gpio_put(board::kDisplayAddress2, (row_ & 0x04u) != 0);
    gpio_set_outover(board::kDisplayOutputEnable,
                     enabled_ && brightness_ > 0
                         ? GPIO_OVERRIDE_NORMAL
                         : GPIO_OVERRIDE_HIGH);

    row_ = (row_ + 1u) & 0x07u;
}

}  // namespace pico_clock
