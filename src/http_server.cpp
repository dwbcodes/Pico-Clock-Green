#include "http_server.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "config_json.hpp"
#include "chime_schedule.hpp"
#include "control_json.hpp"
#include "display_schedule.hpp"
#include "http_headers.hpp"
#include "http_request.hpp"
#include "number_format.hpp"
#include "version.hpp"
#include "wifi_policy.hpp"
#include "lwip/pbuf.h"

namespace pico_clock {

HttpServer::HttpServer() {
    // Raw lwIP callbacks run from the CYW43 background IRQ. A blocking mutex
    // here can deadlock if that IRQ interrupts the main loop while it owns the
    // lock. A critical section prevents that inversion and all protected copies
    // are deliberately short.
    critical_section_init(&state_lock_);
}

bool HttpServer::start(const AppConfig& config, const char* mac_address,
                       bool station_mode) {
    stop();
    current_ = config;
    std::snprintf(mac_address_, sizeof(mac_address_), "%s",
                  mac_address != nullptr ? mac_address : "unknown");
    station_mode_ = station_mode;
    listener_ = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (listener_ == nullptr || tcp_bind(listener_, IP_ANY_TYPE, 80) != ERR_OK) {
        stop();
        return false;
    }

    listener_ = tcp_listen_with_backlog(listener_, 2);
    if (listener_ == nullptr) return false;
    tcp_arg(listener_, this);
    tcp_accept(listener_, accept);
    return true;
}

bool HttpServer::take_sync_request() {
    critical_section_enter_blocking(&state_lock_);
    const bool pending = sync_pending_;
    sync_pending_ = false;
    critical_section_exit(&state_lock_);
    return pending;
}

bool HttpServer::take_control_action(ControlAction& action) {
    critical_section_enter_blocking(&state_lock_);
    const bool pending = action_pending_;
    if (pending) {
        action = pending_action_;
        action_pending_ = false;
    }
    critical_section_exit(&state_lock_);
    return pending;
}

bool HttpServer::take_power_save_test(unsigned& duration_seconds) {
    critical_section_enter_blocking(&state_lock_);
    const bool pending = power_save_test_pending_;
    if (pending) {
        duration_seconds = pending_power_save_test_seconds_;
        power_save_test_pending_ = false;
    }
    critical_section_exit(&state_lock_);
    return pending;
}

bool HttpServer::take_wifi_scan_request() {
    critical_section_enter_blocking(&state_lock_);
    const bool pending = wifi_scan_pending_;
    wifi_scan_pending_ = false;
    critical_section_exit(&state_lock_);
    return pending;
}

void HttpServer::update_wifi_scan(const WifiScanSnapshot& snapshot) {
    critical_section_enter_blocking(&state_lock_);
    wifi_scan_ = snapshot;
    critical_section_exit(&state_lock_);
}

void HttpServer::update_runtime_status(const ControlStatus& status,
                                       float temperature_c, int signal_percent,
                                       bool network_connected,
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
    critical_section_enter_blocking(&state_lock_);
    control_status_ = status;
    temperature_c_ = temperature_c;
    signal_percent_ = signal_percent;
    network_connected_ = network_connected;
    ntp_synchronized_ = ntp_synchronized;
    last_ntp_sync_ = last_ntp_sync;
    rtc_skew_available_ = rtc_skew_available;
    rtc_skew_seconds_ = rtc_skew_seconds;
    ambient_light_percent_ = ambient_light_percent;
    brightness_percent_ = brightness_percent;
    device_metrics_ = device_metrics;
    clock_time_ = clock_time;
    display_frame_ = frame;
    power_save_status_ = power_save_status;
    critical_section_exit(&state_lock_);
}

void HttpServer::stop() {
    if (listener_ != nullptr) {
        tcp_arg(listener_, nullptr);
        tcp_accept(listener_, nullptr);
        if (tcp_close(listener_) != ERR_OK) tcp_abort(listener_);
        listener_ = nullptr;
    }
    for (auto& connection : connections_) {
        if (connection.server != nullptr) close(&connection);
    }
}

bool HttpServer::take_pending_config(AppConfig& config) {
    critical_section_enter_blocking(&state_lock_);
    const bool pending = config_pending_;
    if (pending) {
        config = pending_;
        config_pending_ = false;
    }
    critical_section_exit(&state_lock_);
    return pending;
}

err_t HttpServer::accept(void* argument, tcp_pcb* client, err_t error) {
    if (error != ERR_OK || client == nullptr) return ERR_VAL;
    auto* server = static_cast<HttpServer*>(argument);
    auto* connection = server->allocate_connection(client);
    if (connection == nullptr) {
        tcp_abort(client);
        return ERR_ABRT;
    }

    tcp_arg(client, connection);
    tcp_recv(client, receive);
    tcp_err(client, connection_error);
    return ERR_OK;
}

err_t HttpServer::receive(void* argument, tcp_pcb* client, pbuf* packet, err_t error) {
    auto* connection = static_cast<Connection*>(argument);
    if (packet == nullptr || error != ERR_OK) {
        if (packet != nullptr) pbuf_free(packet);
        close(connection);
        return ERR_OK;
    }

    const auto packet_length = packet->tot_len;
    const auto available = sizeof(connection->request) - connection->length - 1;
    const auto copied = pbuf_copy_partial(packet,
        connection->request + connection->length,
        std::min<std::size_t>(available, packet->tot_len), 0);
    connection->length += copied;
    connection->request[connection->length] = '\0';
    tcp_recved(client, packet_length);
    pbuf_free(packet);

    const char* headers_end = std::strstr(connection->request, "\r\n\r\n");
    if (headers_end == nullptr) {
        if (copied == packet_length) return ERR_OK;
        connection->server->send_response(connection, "413 Payload Too Large",
                                           "text/plain", "Request too large\n");
        return ERR_OK;
    }

    std::size_t content_length = 0;
    if (const char* header = std::strstr(connection->request, "Content-Length:")) {
        content_length = std::strtoul(header + std::strlen("Content-Length:"), nullptr, 10);
    }
    const auto header_size = static_cast<std::size_t>(headers_end + 4 - connection->request);
    if (connection->length < header_size + content_length) return ERR_OK;

    connection->server->handle_request(connection);
    return ERR_OK;
}

void HttpServer::connection_error(void* argument, err_t) {
    auto* connection = static_cast<Connection*>(argument);
    if (connection == nullptr || connection->server == nullptr) return;
    auto* server = connection->server;
    connection->pcb = nullptr;
    server->release_connection(connection);
}

err_t HttpServer::asset_sent(void* argument, tcp_pcb*, std::uint16_t length) {
    auto* connection = static_cast<Connection*>(argument);
    connection->idle_polls = 0;
    connection->asset_outstanding = length >= connection->asset_outstanding
        ? 0 : connection->asset_outstanding - length;
    send_asset_chunks(connection);
    return ERR_OK;
}

err_t HttpServer::asset_poll(void* argument, tcp_pcb*) {
    auto* connection = static_cast<Connection*>(argument);
    if (++connection->idle_polls > 30) {
        close(connection);
        return ERR_OK;
    }
    send_asset_chunks(connection);
    return ERR_OK;
}

void HttpServer::send_asset_chunks(Connection* connection) {
    while (connection->asset_offset < connection->asset_size) {
        const std::size_t available = tcp_sndbuf(connection->pcb);
        if (available == 0) break;
        const std::size_t remaining = connection->asset_size - connection->asset_offset;
        const std::size_t chunk = std::min<std::size_t>(available,
            std::min<std::size_t>(remaining, 4096));
        const u8_t flags = remaining > chunk ? TCP_WRITE_FLAG_MORE : 0;
        // Generated assets live in immutable flash for the lifetime of the
        // connection, so lwIP can transmit them directly without allocating a
        // second copy of each chunk from its small heap.
        const err_t result = tcp_write(connection->pcb,
            connection->asset_data + connection->asset_offset, chunk,
            flags);
        if (result == ERR_MEM) break;
        if (result != ERR_OK) {
            close(connection);
            return;
        }
        connection->asset_offset += chunk;
        connection->asset_outstanding += chunk;
    }
    tcp_output(connection->pcb);
    if (connection->asset_offset == connection->asset_size &&
        connection->asset_outstanding == 0) {
        close(connection);
    }
}

void HttpServer::close(Connection* connection) {
    if (connection == nullptr) return;
    auto* server = connection->server;
    if (connection->pcb != nullptr) {
        tcp_arg(connection->pcb, nullptr);
        tcp_recv(connection->pcb, nullptr);
        tcp_sent(connection->pcb, nullptr);
        tcp_poll(connection->pcb, nullptr, 0);
        tcp_err(connection->pcb, nullptr);
        if (tcp_close(connection->pcb) != ERR_OK) tcp_abort(connection->pcb);
        connection->pcb = nullptr;
    }
    if (server != nullptr) server->release_connection(connection);
}

HttpServer::Connection* HttpServer::allocate_connection(tcp_pcb* client) {
    for (auto& connection : connections_) {
        if (connection.server != nullptr) continue;
        connection = {};
        connection.server = this;
        connection.pcb = client;
        return &connection;
    }
    return nullptr;
}

void HttpServer::release_connection(Connection* connection) {
    if (connection == nullptr) return;
    *connection = {};
}

void HttpServer::handle_request(Connection* connection) {
    if (HttpRequest::matches(connection->request, "GET", "/api/v1") ||
        HttpRequest::matches(connection->request, "GET", "/api/v1/")) {
        send_api_index(connection);
        return;
    }

    if (HttpRequest::matches(connection->request, "GET", "/docs") ||
        HttpRequest::matches(connection->request, "GET", "/docs/")) {
        send_api_docs(connection);
        return;
    }

    if (std::strncmp(connection->request, "GET ", 4) == 0) {
        const char* path_start = connection->request + 4;
        const char* path_end = std::strchr(path_start, ' ');
        if (path_end != nullptr && path_end > path_start &&
            static_cast<std::size_t>(path_end - path_start) < 192) {
            char path[192]{};
            std::memcpy(path, path_start,
                        static_cast<std::size_t>(path_end - path_start));
            if (const auto* asset = web_assets::find(path)) {
                send_asset(connection, *asset);
                return;
            }
        }
    }

    if (HttpRequest::matches(connection->request, "GET", "/api/v1/status")) {
        send_status(connection);
        return;
    }

    if (HttpRequest::matches(connection->request, "GET", "/api/v1/config")) {
        send_config(connection);
        return;
    }

    if (HttpRequest::matches(connection->request, "GET", "/api/v1/display")) {
        send_display(connection);
        return;
    }

    if (HttpRequest::matches(connection->request, "GET", "/api/v1/time")) {
        send_time(connection);
        return;
    }

    if (HttpRequest::matches(connection->request, "GET",
                             "/api/v1/wifi/networks")) {
        send_wifi_networks(connection);
        return;
    }

    if (HttpRequest::matches(connection->request, "PUT", "/api/v1/config")) {
        const char* body = std::strstr(connection->request, "\r\n\r\n");
        AppConfig updated{};
        if (body == nullptr || !ConfigJson::parse(body + 4, current_, updated)) {
            send_problem(connection, "400 Bad Request", "Invalid configuration",
                         "Use a supported timezone, valid HH:MM schedule, brightness from 10 to 100 percent, a supported chime interval, and all required fields.");
            return;
        }
        critical_section_enter_blocking(&state_lock_);
        pending_ = updated;
        config_pending_ = true;
        critical_section_exit(&state_lock_);
        current_ = updated;
        send_response(connection, "202 Accepted", "application/json",
                      "{\"status\":\"accepted\",\"restartRequired\":true}\n");
        return;
    }

    if (HttpRequest::matches(connection->request, "POST", "/api/v1/time/sync")) {
        if (!station_mode_) {
            send_problem(connection, "409 Conflict", "Wi-Fi unavailable",
                         "NTP synchronization requires station mode.");
            return;
        }
        critical_section_enter_blocking(&state_lock_);
        sync_pending_ = true;
        critical_section_exit(&state_lock_);
        send_response(connection, "202 Accepted", "application/json",
                      "{\"status\":\"accepted\"}\n");
        return;
    }

    if (HttpRequest::matches(connection->request, "POST", "/api/v1/wifi/scan")) {
        critical_section_enter_blocking(&state_lock_);
        wifi_scan_pending_ = true;
        critical_section_exit(&state_lock_);
        send_response(connection, "202 Accepted", "application/json",
                      "{\"status\":\"accepted\"}\n");
        return;
    }

    const bool display_action = HttpRequest::matches(
        connection->request, "POST", "/api/v1/display/actions");
    if (display_action) {
        const char* body = std::strstr(connection->request, "\r\n\r\n");
        ControlAction action{};
        const bool valid = body != nullptr &&
            ControlJson::parse_display_action(body + 4, action);
        if (!valid) {
            send_problem(connection, "400 Bad Request", "Invalid action",
                         "Use an action documented by the OpenAPI contract.");
            return;
        }
        critical_section_enter_blocking(&state_lock_);
        pending_action_ = action;
        action_pending_ = true;
        critical_section_exit(&state_lock_);
        send_response(connection, "202 Accepted", "application/json",
                      "{\"status\":\"accepted\"}\n");
        return;
    }

    if (HttpRequest::matches(connection->request, "POST",
                             "/api/v1/power-saving/test")) {
        const char* body = std::strstr(connection->request, "\r\n\r\n");
        unsigned duration_seconds = 0;
        if (body == nullptr || !ControlJson::parse_power_save_test(
                                   body + 4, duration_seconds)) {
            send_problem(connection, "400 Bad Request", "Invalid duration",
                         "Use durationSeconds 0, 5, 10, or 15. Zero runs until a physical button is pressed.");
            return;
        }
        critical_section_enter_blocking(&state_lock_);
        pending_power_save_test_seconds_ = duration_seconds;
        power_save_test_pending_ = true;
        critical_section_exit(&state_lock_);
        send_response(connection, "202 Accepted", "application/json",
                      "{\"status\":\"accepted\"}\n");
        return;
    }

    if (HttpRequest::matches(connection->request, "POST",
                             "/api/v1/network/provisioning")) {
        critical_section_enter_blocking(&state_lock_);
        pending_action_ = ControlAction::StartProvisioning;
        action_pending_ = true;
        critical_section_exit(&state_lock_);
        send_response(connection, "202 Accepted", "application/json",
                      "{\"status\":\"accepted\"}\n");
        return;
    }

    if (HttpRequest::matches(connection->request, "POST",
                             "/api/v1/device/factory-reset")) {
        const char* body = std::strstr(connection->request, "\r\n\r\n");
        if (body == nullptr || !ControlJson::confirms_factory_reset(body + 4)) {
            send_problem(connection, "400 Bad Request", "Confirmation required",
                         "Set confirmation to erase-all-settings.");
            return;
        }
        critical_section_enter_blocking(&state_lock_);
        pending_action_ = ControlAction::FactoryReset;
        action_pending_ = true;
        critical_section_exit(&state_lock_);
        send_response(connection, "202 Accepted", "application/json",
                      "{\"status\":\"accepted\",\"restartRequired\":true}\n");
        return;
    }

    send_problem(connection, "404 Not Found", "Not found",
                 "The requested resource does not exist.");
}

void HttpServer::send_config(Connection* connection) {
    char off[6]{};
    char on[6]{};
    if (!ConfigJson::escape(current_.ssid, escaped_ssid_, sizeof(escaped_ssid_)) ||
        !ConfigJson::escape(current_.ntp_server, escaped_ntp_, sizeof(escaped_ntp_)) ||
        !ConfigJson::escape(current_.timezone, escaped_timezone_,
                            sizeof(escaped_timezone_)) ||
        !DisplaySchedule::format_time(current_.display_off_minute, off, sizeof(off)) ||
        !DisplaySchedule::format_time(current_.display_on_minute, on, sizeof(on))) {
        send_problem(connection, "500 Internal Server Error", "Encoding failure",
                     "Stored configuration could not be encoded.");
        return;
    }
    std::snprintf(response_body_, sizeof(response_body_),
        "{\"ssid\":\"%s\",\"passwordConfigured\":%s,"
        "\"hostname\":\"%s\",\"domainName\":\"%s\","
        "\"ntpServer\":\"%s\",\"timezone\":\"%s\","
        "\"displaySchedule\":{\"enabled\":%s,\"off\":\"%s\","
        "\"on\":\"%s\"},\"automaticBrightness\":%s,"
        "\"manualBrightnessPercent\":%u,\"chimeInterval\":\"%s\"}\n",
        escaped_ssid_, current_.password[0] != '\0' ? "true" : "false",
        current_.hostname, current_.domain_name,
        escaped_ntp_, escaped_timezone_,
        current_.display_schedule_enabled ? "true" : "false", off, on,
        current_.automatic_brightness ? "true" : "false",
        static_cast<unsigned>(current_.manual_brightness_percent),
        chime_interval_name(current_.chime_interval_minutes));
    send_response(connection, "200 OK", "application/json", response_body_);
}

void HttpServer::send_api_index(Connection* connection) {
    send_response(connection, "200 OK", "application/json",
        "{\"title\":\"greenpico API\",\"version\":\"v1\","
        "\"documentation\":\"/docs\",\"resources\":{"
        "\"status\":\"/api/v1/status\",\"config\":\"/api/v1/config\","
        "\"time\":\"/api/v1/time\","
        "\"display\":\"/api/v1/display\","
        "\"displayActions\":\"/api/v1/display/actions\","
        "\"powerSavingTest\":\"/api/v1/power-saving/test\","
        "\"wifiNetworks\":\"/api/v1/wifi/networks\","
        "\"wifiScan\":\"/api/v1/wifi/scan\","
        "\"timeSync\":\"/api/v1/time/sync\","
        "\"provisioning\":\"/api/v1/network/provisioning\","
        "\"factoryReset\":\"/api/v1/device/factory-reset\"}}\n");
}

void HttpServer::send_asset(Connection* connection,
                            const web_assets::Asset& asset) {
    const char* cache_control = std::strcmp(asset.path, "/") == 0 ||
        std::strcmp(asset.path, "/index.html") == 0
        ? "no-store" : "public, max-age=31536000, immutable";
    send_stream(connection, "200 OK", asset.content_type, asset.data,
                asset.size, true, cache_control);
}

void HttpServer::send_stream(Connection* connection, const char* status,
                             const char* content_type,
                             const std::uint8_t* data, std::size_t size,
                             bool gzip, const char* cache_control) {
    const int header_length = HttpHeaders::format(
        response_header_, sizeof(response_header_), status,
        content_type, size, gzip, cache_control);
    if (header_length <= 0 || tcp_write(connection->pcb, response_header_,
            static_cast<std::size_t>(header_length), TCP_WRITE_FLAG_COPY) != ERR_OK) {
        close(connection);
        return;
    }
    connection->asset_data = data;
    connection->asset_size = size;
    connection->asset_offset = 0;
    connection->asset_outstanding = static_cast<std::size_t>(header_length);
    tcp_sent(connection->pcb, asset_sent);
    tcp_poll(connection->pcb, asset_poll, 2);
    send_asset_chunks(connection);
}

void HttpServer::send_api_docs(Connection* connection) {
    static const char body[] =
        "<!doctype html><html><head><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>greenpico API</title><style>"
        "body{font:16px system-ui;max-width:52rem;margin:2rem auto;padding:0 1rem;"
        "background:#101712;color:#e8f5e9}a{color:#76d68d}"
        "code{background:#223629;padding:.15rem .3rem;border-radius:.2rem}"
        "table{width:100%;border-collapse:collapse}th,td{text-align:left;"
        "vertical-align:top;border-bottom:1px solid #456b50;padding:.55rem}</style>"
        "</head><body><h1>greenpico API v1</h1>"
        "<p><a href='/'>Clock settings</a> &middot; "
        "<a href='/api/v1'>Machine-readable API index</a></p>"
        "<p>The API is unauthenticated and intended for a trusted local network.</p>"
        "<table><thead><tr><th>Method</th><th>Path</th><th>Purpose</th></tr></thead><tbody>"
        "<tr><td>GET</td><td><code>/api/v1/status</code></td><td>Device, network, and Pico resource metrics</td></tr>"
        "<tr><td>GET</td><td><code>/api/v1/config</code></td><td>Redacted saved configuration</td></tr>"
        "<tr><td>PUT</td><td><code>/api/v1/config</code></td><td>Replace configuration and restart</td></tr>"
        "<tr><td>GET</td><td><code>/api/v1/time</code></td><td>Current RTC date and local time</td></tr>"
        "<tr><td>GET</td><td><code>/api/v1/display</code></td><td>Page, brightness, temperature, and signal</td></tr>"
        "<tr><td>POST</td><td><code>/api/v1/display/actions</code></td><td>Change page, brightness, units, wake, or sleep</td></tr>"
        "<tr><td>POST</td><td><code>/api/v1/power-saving/test</code></td><td>Blank the display and test aggressive Wi-Fi power management</td></tr>"
        "<tr><td>POST</td><td><code>/api/v1/wifi/scan</code></td><td>Start a wireless network scan</td></tr>"
        "<tr><td>GET</td><td><code>/api/v1/wifi/networks</code></td><td>Read scan state and discovered networks</td></tr>"
        "<tr><td>POST</td><td><code>/api/v1/time/sync</code></td><td>Request immediate NTP synchronization</td></tr>"
        "<tr><td>POST</td><td><code>/api/v1/network/provisioning</code></td><td>Start the temporary setup AP</td></tr>"
        "<tr><td>POST</td><td><code>/api/v1/device/factory-reset</code></td><td>Erase configuration with confirmation</td></tr>"
        "</tbody></table><h2>Display action body</h2>"
        "<p><code>{&quot;action&quot;:&quot;show-temperature&quot;}</code></p>"
        "<p>Actions: <code>next-page</code>, <code>show-time</code>, "
        "<code>show-date</code>, <code>show-temperature</code>, "
        "<code>show-status</code>, <code>next-status</code>, "
        "<code>previous-status</code>, <code>brightness-up</code>, "
        "<code>brightness-down</code>, <code>automatic-brightness</code>, "
        "<code>select-celsius</code>, <code>select-fahrenheit</code>, "
        "<code>wake</code>, "
        "<code>sleep-until-schedule</code>, and <code>maximum-brightness</code>.</p>"
        "<p>The Status page cycles through Wi-Fi, NTP, and Brightness. "
        "Brightness reports ambient light and actual PWM percentages. Persistent "
        "automatic/manual brightness is configured with <code>PUT /api/v1/config</code>.</p>"
        "<h2>Power-saving test body</h2>"
        "<p><code>{&quot;durationSeconds&quot;:5}</code>; use 0 to wait for a physical button.</p>"
        "<h2>Factory-reset body</h2>"
        "<p><code>{&quot;confirmation&quot;:&quot;erase-all-settings&quot;}</code></p>"
        "</body></html>";
    // This page is intentionally larger than the copied dynamic-response
    // buffer. It has static lifetime in flash, so stream it without copying.
    send_stream(connection, "200 OK", "text/html; charset=utf-8",
                reinterpret_cast<const std::uint8_t*>(body), sizeof(body) - 1,
                false, "no-store");
}

void HttpServer::send_status(Connection* connection) {
    DeviceMetrics metrics{};
    critical_section_enter_blocking(&state_lock_);
    metrics = device_metrics_;
    critical_section_exit(&state_lock_);
    if (!format_fqdn(current_.hostname, current_.domain_name, fqdn_,
                     sizeof(fqdn_))) {
        send_problem(connection, "500 Internal Server Error", "Encoding failure",
                     "Stored network identity could not be encoded.");
        return;
    }
    std::snprintf(response_body_, sizeof(response_body_),
        "{\"device\":\"%s\",\"domainName\":\"%s\",\"fqdn\":\"%s\","
        "\"firmwareVersion\":\"%s\","
        "\"networkMode\":\"%s\",\"macAddress\":\"%s\","
        "\"configurationComplete\":%s,"
        "\"metrics\":{\"uptimeSeconds\":%lu,\"cpuFrequencyHz\":%lu,"
        "\"ramTotalBytes\":%lu,\"ramStaticBytes\":%lu,"
        "\"ramAllocatedBytes\":%lu,\"heapUsedBytes\":%lu,"
        "\"heapCapacityBytes\":%lu,\"flashTotalBytes\":%lu,"
        "\"firmwareBytes\":%lu,\"resetReason\":\"%s\"}}\n",
        current_.hostname, current_.domain_name, fqdn_, kFirmwareVersion,
        station_mode_ ? "station" : "provisioning", mac_address_,
        current_.has_wifi() ? "true" : "false",
        static_cast<unsigned long>(metrics.uptime_seconds),
        static_cast<unsigned long>(metrics.cpu_frequency_hz),
        static_cast<unsigned long>(metrics.memory.ram_total_bytes),
        static_cast<unsigned long>(metrics.memory.ram_static_bytes),
        static_cast<unsigned long>(metrics.memory.ram_allocated_bytes),
        static_cast<unsigned long>(metrics.memory.heap_used_bytes),
        static_cast<unsigned long>(metrics.memory.heap_capacity_bytes),
        static_cast<unsigned long>(metrics.flash_total_bytes),
        static_cast<unsigned long>(metrics.firmware_bytes),
        metrics.watchdog_reset ? "watchdog" : "power-on-or-external");
    send_response(connection, "200 OK", "application/json", response_body_);
}

void HttpServer::send_time(Connection* connection) {
    ClockTime clock_time{};
    bool synchronized = false;
    critical_section_enter_blocking(&state_lock_);
    clock_time = clock_time_;
    synchronized = ntp_synchronized_;
    critical_section_exit(&state_lock_);
    response_body_[0] = '\0';
    if (!format_clock_time_json(clock_time, current_.timezone, synchronized,
                                response_body_, sizeof(response_body_))) {
        send_problem(connection, "500 Internal Server Error", "Encoding failure",
                     "Current clock time could not be encoded.");
        return;
    }
    send_response(connection, "200 OK", "application/json", response_body_);
}

void HttpServer::send_display(Connection* connection) {
    ControlStatus status{};
    float temperature = 0;
    int signal = 0;
    bool connected = false;
    bool synchronized = false;
    ClockTime last_ntp_sync{};
    bool rtc_skew_available = false;
    std::int32_t rtc_skew_seconds = 0;
    int ambient_light = 0;
    int brightness = 0;
    ClockFace::Frame frame{};
    PowerSaveStatus power_save{};
    critical_section_enter_blocking(&state_lock_);
    status = control_status_;
    temperature = temperature_c_;
    signal = signal_percent_;
    connected = network_connected_;
    synchronized = ntp_synchronized_;
    last_ntp_sync = last_ntp_sync_;
    rtc_skew_available = rtc_skew_available_;
    rtc_skew_seconds = rtc_skew_seconds_;
    ambient_light = ambient_light_percent_;
    brightness = brightness_percent_;
    frame = display_frame_;
    power_save = power_save_status_;
    critical_section_exit(&state_lock_);
    const char* active_ssid = station_mode_ ? current_.ssid : kDeviceName;
    if (!ConfigJson::escape(active_ssid, escaped_ssid_, sizeof(escaped_ssid_)) ||
        !ConfigJson::escape(current_.ntp_server, escaped_ntp_,
                            sizeof(escaped_ntp_))) {
        send_problem(connection, "500 Internal Server Error", "Encoding failure",
                     "Stored status text could not be encoded.");
        return;
    }
    char temperature_number[16]{};
    if (!NumberFormat::fixed_two(temperature, temperature_number,
                                 sizeof(temperature_number))) {
        send_problem(connection, "500 Internal Server Error", "Encoding failure",
                     "Temperature could not be encoded.");
        return;
    }
    char last_sync_json[48]{};
    if (!format_clock_time_iso_json_value(last_ntp_sync, last_sync_json,
                                          sizeof(last_sync_json))) {
        send_problem(connection, "500 Internal Server Error", "Encoding failure",
                     "Last NTP synchronization time could not be encoded.");
        return;
    }
    char rtc_skew_json[24]{"null"};
    if (rtc_skew_available) {
        std::snprintf(rtc_skew_json, sizeof(rtc_skew_json), "%ld",
                      static_cast<long>(rtc_skew_seconds));
    }
    std::snprintf(response_body_, sizeof(response_body_),
        "{\"page\":\"%s\",\"statusView\":\"%s\","
        "\"powerOverride\":\"%s\","
        "\"automaticBrightness\":%s,\"brightnessBias\":%d,"
        "\"temperatureUnit\":\"%s\",\"temperatureCelsius\":%s,"
        "\"networkConnected\":%s,\"wifiSsid\":\"%s\","
        "\"signalPercent\":%d,\"ntpPeer\":\"%s\","
        "\"ntpSynchronized\":%s,\"lastNtpSync\":%s,"
        "\"rtcSkewSeconds\":%s,"
        "\"ambientLightPercent\":%d,\"brightnessPercent\":%d,"
        "\"powerSaving\":%s,\"powerSaveTestActive\":%s,"
        "\"powerSaveTestDurationSeconds\":%u,"
        "\"powerSaveTestSecondsRemaining\":%u,"
        "\"frame\":[\"%06lX\",\"%06lX\",\"%06lX\",\"%06lX\","
        "\"%06lX\",\"%06lX\",\"%06lX\",\"%06lX\"],"
        "\"factoryResetCountdownSeconds\":%u}\n",
        ClockControl::page_name(status.page),
        ClockControl::status_view_name(status.status_view),
        ClockControl::override_name(status.display_override),
        status.automatic_brightness ? "true" : "false",
        status.brightness_bias, ClockControl::unit_name(status.temperature_unit),
        temperature_number, connected ? "true" : "false",
        escaped_ssid_, signal, escaped_ntp_,
        synchronized ? "true" : "false", last_sync_json, rtc_skew_json,
        ambient_light,
        brightness,
        power_save.active ? "true" : "false",
        power_save.test_active ? "true" : "false",
        static_cast<unsigned>(power_save.test_duration_seconds),
        static_cast<unsigned>(power_save.test_seconds_remaining),
        static_cast<unsigned long>((frame[0] >> 8) & 0xffffffu),
        static_cast<unsigned long>((frame[1] >> 8) & 0xffffffu),
        static_cast<unsigned long>((frame[2] >> 8) & 0xffffffu),
        static_cast<unsigned long>((frame[3] >> 8) & 0xffffffu),
        static_cast<unsigned long>((frame[4] >> 8) & 0xffffffu),
        static_cast<unsigned long>((frame[5] >> 8) & 0xffffffu),
        static_cast<unsigned long>((frame[6] >> 8) & 0xffffffu),
        static_cast<unsigned long>((frame[7] >> 8) & 0xffffffu),
        static_cast<unsigned>(status.factory_reset_countdown_seconds));
    send_response(connection, "200 OK", "application/json", response_body_);
}

void HttpServer::send_wifi_networks(Connection* connection) {
    critical_section_enter_blocking(&state_lock_);
    response_wifi_scan_ = wifi_scan_;
    critical_section_exit(&state_lock_);

    std::size_t used = static_cast<std::size_t>(std::snprintf(
        response_body_, sizeof(response_body_),
        "{\"state\":\"%s\",\"networks\":[",
        wifi_scan_state_name(response_wifi_scan_.state)));
    // SSIDs can expand by at most two characters per input byte with the
    // supported JSON escapes.
    bool first = true;
    for (std::size_t index = 0; index < response_wifi_scan_.count; ++index) {
        if (!ConfigJson::escape(response_wifi_scan_.networks[index].ssid,
                                escaped_network_, sizeof(escaped_network_))) continue;
        const int written = std::snprintf(
            response_body_ + used, sizeof(response_body_) - used,
            "%s{\"ssid\":\"%s\",\"rssi\":%d,\"signalPercent\":%d,"
            "\"channel\":%u,\"security\":\"%s\"}",
            first ? "" : ",", escaped_network_,
            response_wifi_scan_.networks[index].rssi,
            wifi_signal_percent(response_wifi_scan_.networks[index].rssi),
            static_cast<unsigned>(response_wifi_scan_.networks[index].channel),
            response_wifi_scan_.networks[index].secured ? "secured" : "open");
        if (written < 0 || static_cast<std::size_t>(written) >=
                sizeof(response_body_) - used) {
            send_problem(connection, "500 Internal Server Error", "Encoding failure",
                         "Wi-Fi scan results exceeded the response buffer.");
            return;
        }
        used += static_cast<std::size_t>(written);
        first = false;
    }
    std::snprintf(response_body_ + used, sizeof(response_body_) - used, "]}\n");
    send_response(connection, "200 OK", "application/json", response_body_);
}

void HttpServer::send_problem(Connection* connection, const char* status,
                              const char* title, const char* detail) {
    std::snprintf(response_body_, sizeof(response_body_),
        "{\"type\":\"about:blank\",\"title\":\"%s\","
        "\"status\":%d,\"detail\":\"%s\"}\n",
        title, std::atoi(status), detail);
    send_response(connection, status, "application/problem+json", response_body_);
}

void HttpServer::send_response(Connection* connection, const char* status,
                               const char* content_type, const char* body) {
    const auto body_length = std::strlen(body);
    if (body_length >= sizeof(connection->request)) {
        close(connection);
        return;
    }
    std::memmove(connection->request, body, body_length);
    send_stream(connection, status, content_type,
                reinterpret_cast<const std::uint8_t*>(connection->request),
                body_length, false, "no-store");
}

}  // namespace pico_clock
