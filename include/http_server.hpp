#pragma once

#include <cstddef>

#include "app_config.hpp"
#include "clock_control.hpp"
#include "clock_face.hpp"
#include "clock_time.hpp"
#include "device_metrics.hpp"
#include "power_save.hpp"
#include "wifi_scan.hpp"
#include "generated_web_assets.hpp"
#include "lwip/tcp.h"
#include "pico/critical_section.h"

namespace pico_clock {

class HttpServer {
public:
    HttpServer();
    bool start(const AppConfig& config, const char* mac_address,
               bool station_mode);
    void stop();
    bool take_pending_config(AppConfig& config);
    bool take_sync_request();
    bool take_control_action(ControlAction& action);
    bool take_power_save_test(unsigned& duration_seconds);
    bool take_wifi_scan_request();
    void update_wifi_scan(const WifiScanSnapshot& snapshot);
    void update_runtime_status(const ControlStatus& status, float temperature_c,
                               int signal_percent, bool network_connected,
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

private:
    struct Connection {
        HttpServer* server{nullptr};
        tcp_pcb* pcb{nullptr};
        // Reused as stable response storage after request parsing. Keeping the
        // bytes with the connection lets lwIP stream them without copying or
        // closing the PCB before acknowledgements arrive.
        char request[3072]{};
        std::size_t length{0};
        const std::uint8_t* asset_data{nullptr};
        std::size_t asset_size{0};
        std::size_t asset_offset{0};
        std::size_t asset_outstanding{0};
        std::uint8_t idle_polls{0};
    };

    static err_t accept(void* argument, tcp_pcb* client, err_t error);
    static err_t receive(void* argument, tcp_pcb* client, pbuf* packet, err_t error);
    static void connection_error(void* argument, err_t error);
    static err_t asset_sent(void* argument, tcp_pcb* client,
                            std::uint16_t length);
    static err_t asset_poll(void* argument, tcp_pcb* client);
    static void send_asset_chunks(Connection* connection);
    static void close(Connection* connection);
    Connection* allocate_connection(tcp_pcb* client);
    void release_connection(Connection* connection);

    void handle_request(Connection* connection);
    void send_api_index(Connection* connection);
    void send_api_docs(Connection* connection);
    void send_asset(Connection* connection, const web_assets::Asset& asset);
    void send_config(Connection* connection);
    void send_status(Connection* connection);
    void send_time(Connection* connection);
    void send_display(Connection* connection);
    void send_wifi_networks(Connection* connection);
    void send_problem(Connection* connection, const char* status,
                      const char* title, const char* detail);
    void send_stream(Connection* connection, const char* status,
                     const char* content_type, const std::uint8_t* data,
                     std::size_t size, bool gzip, const char* cache_control);
    void send_response(Connection* connection, const char* status,
                       const char* content_type, const char* body);
    tcp_pcb* listener_{nullptr};
    static constexpr std::size_t kMaximumConnections = 2;
    Connection connections_[kMaximumConnections]{};
    AppConfig current_{};
    AppConfig pending_{};
    char mac_address_[18]{"unknown"};
    volatile bool config_pending_{false};
    volatile bool sync_pending_{false};
    volatile bool action_pending_{false};
    volatile bool power_save_test_pending_{false};
    volatile bool wifi_scan_pending_{false};
    ControlAction pending_action_{ControlAction::NextPage};
    unsigned pending_power_save_test_seconds_{0};
    ControlStatus control_status_{};
    float temperature_c_{0.0f};
    int signal_percent_{0};
    bool network_connected_{false};
    bool ntp_synchronized_{false};
    ClockTime last_ntp_sync_{};
    bool rtc_skew_available_{false};
    std::int32_t rtc_skew_seconds_{0};
    int ambient_light_percent_{0};
    int brightness_percent_{0};
    DeviceMetrics device_metrics_{};
    ClockTime clock_time_{};
    ClockFace::Frame display_frame_{};
    PowerSaveStatus power_save_status_{};
    WifiScanSnapshot wifi_scan_{};
    bool station_mode_{false};
    critical_section_t state_lock_{};

    // Raw lwIP callbacks execute on the interrupted main-loop stack. Keep the
    // comparatively large HTTP formatting workspace in static storage owned
    // by the server instead of consuming that stack. Callbacks are serialized
    // by lwIP, and response writes use TCP_WRITE_FLAG_COPY before returning.
    char response_header_[320]{};
    char response_body_[3072]{};
    char escaped_ssid_[AppConfig::kSsidSize * 2]{};
    char escaped_ntp_[AppConfig::kNtpServerSize * 2]{};
    char escaped_timezone_[Timezone::kNameSize * 2]{};
    char fqdn_[AppConfig::kFqdnSize]{};
    char escaped_network_[67]{};
    WifiScanSnapshot response_wifi_scan_{};
};

}  // namespace pico_clock
