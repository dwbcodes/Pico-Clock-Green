#include "ds3231.hpp"

#include <array>

#include "board.hpp"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

namespace pico_clock {

unsigned char Ds3231::to_bcd(int value) {
    return static_cast<unsigned char>(((value / 10) << 4) | (value % 10));
}

int Ds3231::from_bcd(unsigned char value) {
    return ((value >> 4) * 10) + (value & 0x0f);
}

bool Ds3231::init() {
    i2c_init(board::kRtcI2c, 400000);
    gpio_set_function(board::kRtcSda, GPIO_FUNC_I2C);
    gpio_set_function(board::kRtcScl, GPIO_FUNC_I2C);
    gpio_pull_up(board::kRtcSda);
    gpio_pull_up(board::kRtcScl);

    const unsigned char register_address = 0x0f;
    unsigned char status = 0;
    if (i2c_write_blocking(board::kRtcI2c, board::kRtcAddress,
                           &register_address, 1, true) != 1 ||
        i2c_read_blocking(board::kRtcI2c, board::kRtcAddress, &status, 1, false) != 1) {
        return false;
    }

    if ((status & 0x80u) != 0) {
        const unsigned char clear_status[] = {0x0f,
            static_cast<unsigned char>(status & ~0x80u)};
        i2c_write_blocking(board::kRtcI2c, board::kRtcAddress,
                           clear_status, sizeof(clear_status), false);
    }
    return true;
}

bool Ds3231::read(datetime_t& value) const {
    const unsigned char register_address = 0x00;
    std::array<unsigned char, 7> data{};
    if (i2c_write_blocking(board::kRtcI2c, board::kRtcAddress,
                           &register_address, 1, true) != 1 ||
        i2c_read_blocking(board::kRtcI2c, board::kRtcAddress,
                          data.data(), data.size(), false) != static_cast<int>(data.size())) {
        return false;
    }

    value.sec = static_cast<std::int8_t>(from_bcd(data[0] & 0x7f));
    value.min = static_cast<std::int8_t>(from_bcd(data[1] & 0x7f));
    value.hour = static_cast<std::int8_t>(from_bcd(data[2] & 0x3f));
    value.dotw = static_cast<std::int8_t>((data[3] - 1) % 7);
    value.day = static_cast<std::int8_t>(from_bcd(data[4] & 0x3f));
    value.month = static_cast<std::int8_t>(from_bcd(data[5] & 0x1f));
    value.year = static_cast<std::int16_t>(2000 + from_bcd(data[6]));
    return true;
}

bool Ds3231::write(const datetime_t& value) const {
    const std::array<unsigned char, 8> data{
        0x00,
        to_bcd(value.sec),
        to_bcd(value.min),
        to_bcd(value.hour),
        static_cast<unsigned char>((value.dotw % 7) + 1),
        to_bcd(value.day),
        to_bcd(value.month),
        to_bcd(value.year % 100),
    };
    return i2c_write_blocking(board::kRtcI2c, board::kRtcAddress,
                              data.data(), data.size(), false) == static_cast<int>(data.size());
}

bool Ds3231::read_temperature(float& celsius) const {
    const unsigned char register_address = 0x11;
    std::array<unsigned char, 2> data{};
    if (i2c_write_blocking(board::kRtcI2c, board::kRtcAddress,
                           &register_address, 1, true) != 1 ||
        i2c_read_blocking(board::kRtcI2c, board::kRtcAddress,
                          data.data(), data.size(), false) != static_cast<int>(data.size())) {
        return false;
    }

    celsius = static_cast<float>(static_cast<signed char>(data[0])) +
              static_cast<float>(data[1] >> 6) * 0.25f;
    return true;
}

}  // namespace pico_clock
