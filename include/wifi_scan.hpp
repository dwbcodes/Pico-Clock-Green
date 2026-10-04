#pragma once

#include <cstddef>
#include <cstdint>

namespace pico_clock {

enum class WifiScanState : std::uint8_t { Idle, Scanning, Complete, Failed };

struct WifiNetwork {
    char ssid[33]{};
    std::int16_t rssi{-100};
    std::uint8_t channel{0};
    bool secured{false};
};

struct WifiScanSnapshot {
    static constexpr std::size_t kMaximumNetworks = 12;

    WifiScanState state{WifiScanState::Idle};
    WifiNetwork networks[kMaximumNetworks]{};
    std::size_t count{0};
};

inline int wifi_signal_percent(std::int16_t rssi) {
    return rssi <= -90 ? 0 : (rssi >= -40 ? 100 : (rssi + 90) * 2);
}

inline const char* wifi_scan_state_name(WifiScanState state) {
    switch (state) {
        case WifiScanState::Idle: return "idle";
        case WifiScanState::Scanning: return "scanning";
        case WifiScanState::Complete: return "complete";
        case WifiScanState::Failed: return "failed";
    }
    return "failed";
}

}  // namespace pico_clock
