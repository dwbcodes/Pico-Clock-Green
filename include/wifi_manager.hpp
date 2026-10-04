#pragma once

#include <cstdint>

#include "app_config.hpp"
#include "clock_control.hpp"
#include "clock_time.hpp"
#include "device_metrics.hpp"
#include "dhcp_server.hpp"
#include "http_server.hpp"
#include "mac_address.hpp"
#include "power_save.hpp"
#include "wifi_scan.hpp"
#include "pico/critical_section.h"
#include "pico/time.h"

struct _cyw43_ev_scan_result_t;

namespace pico_clock {

class WifiManager {
public:
    enum class Mode { Disconnected, Station, Provisioning };

    bool init(const AppConfig& config);
    void poll();
    Mode mode() const;
    bool take_pending_config(AppConfig& config);
    bool take_sync_request();
    bool take_control_action(ControlAction& action);
    bool take_power_save_test(unsigned& duration_seconds);
    void update_runtime_status(const ControlStatus& status, float temperature_c,
                               bool ntp_synchronized,
                               const ClockTime& last_ntp_sync,
                               bool rtc_skew_available,
                               std::int32_t rtc_skew_seconds,
                               int ambient_light_percent,
                               int brightness_percent,
                               const DeviceMetrics& device_metrics,
                               const ClockTime& clock_time,
                               const ClockFace::Frame& frame,
                               const PowerSaveStatus& power_save_status);
    bool force_provisioning(const AppConfig& config);
    bool force_station(const AppConfig& config);
    int signal_percent() const;
    bool set_power_saving(bool enabled);

private:
    bool connect_station(const AppConfig& config);
    bool start_provisioning(const AppConfig& config);
    void start_http(const AppConfig& config);
    void start_scan();
    static int scan_result(void* environment,
                           const ::_cyw43_ev_scan_result_t* result);

    Mode mode_{Mode::Disconnected};
    DhcpServer dhcp_{};
    HttpServer http_{};
    std::uint8_t mac_[kMacAddressSize]{};
    char mac_address_[kFormattedMacAddressSize]{"unknown"};
    char station_hostname_[AppConfig::kHostnameSize]{};
    char station_fqdn_[AppConfig::kFqdnSize]{};
    int signal_percent_{0};
    absolute_time_t next_signal_sample_{};
    WifiScanSnapshot wifi_scan_{};
    critical_section_t scan_lock_{};
    bool scan_lock_initialized_{false};
    bool scan_active_{false};
    bool power_saving_{false};
    bool power_saving_requested_{false};
};

}  // namespace pico_clock
