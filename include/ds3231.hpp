#pragma once

#include "pico/types.h"

namespace pico_clock {

class Ds3231 {
public:
    bool init();
    bool read(datetime_t& value) const;
    bool write(const datetime_t& value) const;
    bool read_temperature(float& celsius) const;

private:
    static unsigned char to_bcd(int value);
    static int from_bcd(unsigned char value);
};

}  // namespace pico_clock
