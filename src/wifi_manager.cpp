#include "wifi_manager.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "pico/cyw43_arch.h"
#include "cyw43.h"
#include "cyw43_configport.h"
#include "wifi_policy.hpp"

namespace pico_clock {
namespace {

constexpr std::uint32_t kConnectTimeoutMs = 20000;
constexpr std::uint32_t kSignalSampleIntervalMs = 5000;

}  // namespace

bool WifiManager::init(const AppConfig& config) {
    if (cyw43_arch_init() != 0) {
        std::printf("Wi-Fi initialization failed\n");
        return false;
    }
    if (!scan_lock_initialized_) {
        critical_section_init(&scan_lock_);
        scan_lock_initialized_ = true;
    }

    const int mac_result = cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac_);
    if (mac_result != 0 || !is_valid_unicast_mac(mac_)) {
        std::printf("Wi-Fi MAC unavailable (%d); generating one from the board ID\n",
                    mac_result);
        cyw43_hal_generate_laa_mac(CYW43_HAL_MAC_WLAN0, mac_);
        if (!is_valid_unicast_mac(mac_)) {
            std::printf("Could not generate a valid Wi-Fi MAC address\n");
            return false;
        }

        // The lwIP interfaces copy this field when station or AP mode is enabled.
        std::memcpy(cyw43_state.mac, mac_, sizeof(mac_));
    }
    format_mac_address(mac_, mac_address_);
    std::printf("Pico Wi-Fi MAC: %s\n", mac_address_);

    const bool has_wifi_config = config.has_wifi();
    const bool station_connected = has_wifi_config && connect_station(config);
    if (startup_network_mode(has_wifi_config, station_connected) ==
        StartupNetworkMode::Station) {
        return true;
    }
    return start_provisioning(config);
}

void WifiManager::poll() {
    // The threadsafe-background CYW43 architecture services lwIP callbacks.
    if (mode_ == Mode::Station && time_reached(next_signal_sample_)) {
        std::int32_t rssi = -100;
        if (cyw43_wifi_get_rssi(&cyw43_state, &rssi) == 0) {
            signal_percent_ = wifi_signal_percent(
                static_cast<std::int16_t>(rssi));
        }
        next_signal_sample_ = make_timeout_time_ms(kSignalSampleIntervalMs);
    }

    if (http_.take_wifi_scan_request() && !scan_active_) start_scan();
    if (scan_active_ && !cyw43_wifi_scan_active(&cyw43_state)) {
        WifiScanSnapshot completed{};
        critical_section_enter_blocking(&scan_lock_);
        scan_active_ = false;
        wifi_scan_.state = WifiScanState::Complete;
        std::sort(wifi_scan_.networks, wifi_scan_.networks + wifi_scan_.count,
                  [](const WifiNetwork& left, const WifiNetwork& right) {
                      return left.rssi > right.rssi;
                  });
        completed = wifi_scan_;
        critical_section_exit(&scan_lock_);
        http_.update_wifi_scan(completed);
        if (mode_ == Mode::Provisioning) cyw43_arch_disable_sta_mode();
    }
}

WifiManager::Mode WifiManager::mode() const {
    return mode_;
}

bool WifiManager::take_pending_config(AppConfig& config) {
    return http_.take_pending_config(config);
}

bool WifiManager::take_sync_request() {
    return http_.take_sync_request();
}

bool WifiManager::take_control_action(ControlAction& action) {
    return http_.take_control_action(action);
}

bool WifiManager::take_power_save_test(unsigned& duration_seconds) {
    return http_.take_power_save_test(duration_seconds);
}

void WifiManager::update_runtime_status(const ControlStatus& status,
                                        float temperature_c,
                                        bool ntp_synchronized,
                                        const ClockTime& last_ntp_sync,
                                        bool rtc_skew_available,
                                        std::int32_t rtc_skew_seconds,
                                        int ambient_light_percent,
                                        int brightness_percent,
                                        const DeviceMetrics& device_metrics,
                                        const ClockTime& clock_time,
                                        const ClockFace::Frame& frame,
                                        const PowerSaveStatus& power_save_status) {
    http_.update_runtime_status(status, temperature_c, signal_percent_,
                                mode_ == Mode::Station, ntp_synchronized,
                                last_ntp_sync, rtc_skew_available,
                                rtc_skew_seconds,
                                ambient_light_percent, brightness_percent,
                                device_metrics, clock_time, frame,
                                power_save_status);
}

bool WifiManager::force_provisioning(const AppConfig& config) {
    set_power_saving(false);
    return start_provisioning(config);
}

bool WifiManager::force_station(const AppConfig& config) {
    if (!config.has_wifi()) return false;
    if (connect_station(config)) return true;
    start_provisioning(config);
    return false;
}

int WifiManager::signal_percent() const {
    return signal_percent_;
}

bool WifiManager::set_power_saving(bool enabled) {
    if (mode_ != Mode::Station) {
        power_saving_ = false;
        power_saving_requested_ = false;
        return !enabled;
    }
    if (power_saving_requested_ == enabled) return power_saving_ == enabled;
    power_saving_requested_ = enabled;
    const int result = cyw43_wifi_pm(
        &cyw43_state, enabled ? CYW43_AGGRESSIVE_PM : CYW43_DEFAULT_PM);
    if (result != 0) {
        std::printf("Could not %s CYW43 aggressive power management (%d)\n",
                    enabled ? "enable" : "disable", result);
        return false;
    }
    power_saving_ = enabled;
    std::printf("CYW43 power management: %s\n",
                enabled ? "aggressive" : "normal");
    return true;
}

void WifiManager::start_scan() {
    if (mode_ == Mode::Provisioning) cyw43_arch_enable_sta_mode();

    critical_section_enter_blocking(&scan_lock_);
    wifi_scan_ = {};
    wifi_scan_.state = WifiScanState::Scanning;
    critical_section_exit(&scan_lock_);
    http_.update_wifi_scan(wifi_scan_);

    cyw43_wifi_scan_options_t options{};
    const int result = cyw43_wifi_scan(&cyw43_state, &options, this, scan_result);
    if (result == 0) {
        scan_active_ = true;
        return;
    }

    critical_section_enter_blocking(&scan_lock_);
    wifi_scan_.state = WifiScanState::Failed;
    critical_section_exit(&scan_lock_);
    http_.update_wifi_scan(wifi_scan_);
    if (mode_ == Mode::Provisioning) cyw43_arch_disable_sta_mode();
}

int WifiManager::scan_result(void* environment,
                             const cyw43_ev_scan_result_t* result) {
    if (environment == nullptr || result == nullptr || result->ssid_len == 0) {
        return 0;
    }
    auto* manager = static_cast<WifiManager*>(environment);
    critical_section_enter_blocking(&manager->scan_lock_);

    char ssid[33]{};
    const std::size_t length = std::min<std::size_t>(result->ssid_len, 32);
    std::memcpy(ssid, result->ssid, length);
    std::size_t existing = manager->wifi_scan_.count;
    for (std::size_t index = 0; index < manager->wifi_scan_.count; ++index) {
        if (std::strcmp(manager->wifi_scan_.networks[index].ssid, ssid) == 0) {
            existing = index;
            break;
        }
    }

    if (existing < manager->wifi_scan_.count &&
        result->rssi <= manager->wifi_scan_.networks[existing].rssi) {
        critical_section_exit(&manager->scan_lock_);
        return 0;
    }
    if (existing == manager->wifi_scan_.count) {
        if (manager->wifi_scan_.count >= WifiScanSnapshot::kMaximumNetworks) {
            critical_section_exit(&manager->scan_lock_);
            return 0;
        }
        ++manager->wifi_scan_.count;
    }

    auto& network = manager->wifi_scan_.networks[existing];
    std::memcpy(network.ssid, ssid, sizeof(network.ssid));
    network.rssi = result->rssi;
    network.channel = static_cast<std::uint8_t>(result->channel);
    network.secured = result->auth_mode != CYW43_AUTH_OPEN;
    critical_section_exit(&manager->scan_lock_);
    return 0;
}

bool WifiManager::connect_station(const AppConfig& config) {
    // A watchdog restart does not necessarily reset the external CYW43 radio.
    // Explicitly remove a setup AP left by the previous boot before joining the
    // configured network.
    dhcp_.stop();
    http_.stop();
    cyw43_arch_disable_ap_mode();
    cyw43_arch_enable_sta_mode();
    std::snprintf(station_hostname_, sizeof(station_hostname_), "%s",
                  config.hostname);
    format_fqdn(config.hostname, config.domain_name, station_fqdn_,
                sizeof(station_fqdn_));
    cyw43_arch_lwip_begin();
    netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], station_hostname_);
    cyw43_arch_lwip_end();
    std::printf("Connecting to Wi-Fi SSID '%s'...\n", config.ssid);
    const auto authentication = config.password[0] == '\0'
        ? CYW43_AUTH_OPEN
        : CYW43_AUTH_WPA2_MIXED_PSK;
    const int result = cyw43_arch_wifi_connect_timeout_ms(
        config.ssid, config.password, authentication, kConnectTimeoutMs);
    if (result != 0) {
        std::printf("Wi-Fi connection failed (%d); starting setup access point\n", result);
        cyw43_arch_disable_sta_mode();
        return false;
    }

    mode_ = Mode::Station;
    power_saving_ = false;
    power_saving_requested_ = false;
    next_signal_sample_ = get_absolute_time();
    const auto* address = netif_ip4_addr(&cyw43_state.netif[CYW43_ITF_STA]);
    std::printf("Wi-Fi connected; configuration UI: http://%s/\n",
                ip4addr_ntoa(address));
    std::printf("Network identity: %s (DHCP hostname %s)\n",
                station_fqdn_, station_hostname_);
    start_http(config);
    return true;
}

bool WifiManager::start_provisioning(const AppConfig& config) {
    // Provisioning is the fallback only: ensure a failed station connection is
    // fully down before exposing the setup network.
    cyw43_arch_disable_sta_mode();
    cyw43_arch_disable_ap_mode();
    cyw43_arch_enable_ap_mode(kDeviceName, nullptr, CYW43_AUTH_OPEN);
    auto* interface = &cyw43_state.netif[CYW43_ITF_AP];
    const auto gateway = *netif_ip4_addr(interface);
    const auto netmask = *netif_ip4_netmask(interface);

    cyw43_arch_lwip_begin();
    const bool dhcp_started = dhcp_.start(gateway, netmask);
    cyw43_arch_lwip_end();
    if (!dhcp_started) {
        std::printf("DHCP server failed to start\n");
        return false;
    }

    mode_ = Mode::Provisioning;
    power_saving_ = false;
    power_saving_requested_ = false;
    next_signal_sample_ = at_the_end_of_time;
    std::printf("Setup access point: %s\n", kDeviceName);
    std::printf("Configuration UI: http://%s/\n", ip4addr_ntoa(&gateway));
    start_http(config);
    return true;
}

void WifiManager::start_http(const AppConfig& config) {
    cyw43_arch_lwip_begin();
    const bool started = http_.start(config, mac_address_, mode_ == Mode::Station);
    cyw43_arch_lwip_end();
    std::printf("HTTP configuration server %s\n", started ? "started" : "failed");
}

}  // namespace pico_clock
