#pragma once

namespace pico_clock {

inline constexpr char kDeviceName[] = "greenpico";

enum class StartupNetworkMode { Station, Provisioning };

constexpr StartupNetworkMode startup_network_mode(bool has_wifi_config,
                                                  bool station_connected) {
    return has_wifi_config && station_connected
        ? StartupNetworkMode::Station
        : StartupNetworkMode::Provisioning;
}

}  // namespace pico_clock
