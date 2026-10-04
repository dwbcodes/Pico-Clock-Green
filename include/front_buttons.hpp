#pragma once

#include <cstdint>

#include "button_debouncer.hpp"

namespace pico_clock {

struct FrontButtonEvents {
    bool set_pressed{false};
    bool up_pressed{false};
    bool down_pressed{false};
    bool set_down{false};
    bool up_down{false};
    bool down_down{false};
};

class FrontButtons {
public:
    void init();
    FrontButtonEvents poll(std::uint32_t now_ms);

private:
    ButtonDebouncer set_{};
    ButtonDebouncer up_{};
    ButtonDebouncer down_{};
};

}  // namespace pico_clock
