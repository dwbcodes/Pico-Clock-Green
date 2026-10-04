#include "front_buttons.hpp"

#include "board.hpp"
#include "hardware/gpio.h"

namespace pico_clock {
namespace {

void init_button(unsigned pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

bool is_pressed(unsigned pin) {
    return gpio_get(pin) == 0;
}

}  // namespace

void FrontButtons::init() {
    init_button(board::kButtonSet);
    init_button(board::kButtonUp);
    init_button(board::kButtonDown);
}

FrontButtonEvents FrontButtons::poll(std::uint32_t now_ms) {
    const bool set_pressed = set_.update(is_pressed(board::kButtonSet), now_ms) ==
        ButtonEvent::Pressed;
    const bool up_pressed = up_.update(is_pressed(board::kButtonUp), now_ms) ==
        ButtonEvent::Pressed;
    const bool down_pressed = down_.update(is_pressed(board::kButtonDown), now_ms) ==
        ButtonEvent::Pressed;
    return {set_pressed, up_pressed, down_pressed,
            set_.pressed(), up_.pressed(), down_.pressed()};
}

}  // namespace pico_clock
