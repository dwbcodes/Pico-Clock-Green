#pragma once

#include "hardware/i2c.h"

namespace pico_clock::board {

inline constexpr unsigned kButtonSet = 2;
inline constexpr unsigned kRtcSda = 6;
inline constexpr unsigned kRtcScl = 7;
inline constexpr unsigned kDisplayClock = 10;
inline constexpr unsigned kDisplayData = 11;
inline constexpr unsigned kDisplayLatch = 12;
inline constexpr unsigned kDisplayOutputEnable = 13;
inline constexpr unsigned kBuzzer = 14;
inline constexpr unsigned kButtonDown = 15;
inline constexpr unsigned kDisplayAddress0 = 16;
inline constexpr unsigned kButtonUp = 17;
inline constexpr unsigned kDisplayAddress1 = 18;
inline constexpr unsigned kDisplayAddress2 = 22;
inline constexpr unsigned kAmbientLight = 26;

inline i2c_inst_t* const kRtcI2c = i2c1;
inline constexpr unsigned kRtcAddress = 0x68;

}  // namespace pico_clock::board
