#include <algorithm>
#include <cstdio>
#include <ctime>
#include <cstdint>
#include <limits>
#include <malloc.h>

#include "app_config.hpp"
#include "board.hpp"
#include "buzzer.hpp"
#include "chime_schedule.hpp"
#include "clock_face.hpp"
#include "clock_control.hpp"
#include "clock_time.hpp"
#include "ds3231.hpp"
#include "display_schedule.hpp"
#include "device_metrics.hpp"
#include "led_display.hpp"
#include "front_buttons.hpp"
#include "ntp_client.hpp"
#include "power_save.hpp"
#include "status_text.hpp"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/regs/addressmap.h"
#include "wifi_manager.hpp"
#include "wifi_policy.hpp"
#include "timezone.hpp"
#include "version.hpp"

namespace {

extern "C" {
extern std::uint8_t __end__;
extern std::uint8_t __HeapLimit;
extern std::uint8_t __StackTop;
extern std::uint8_t __flash_binary_end;
}

using pico_clock::AppConfig;
using pico_clock::Buzzer;
using pico_clock::ConfigStore;
using pico_clock::ClockFace;
using pico_clock::ClockControl;
using pico_clock::ClockTime;
using pico_clock::ControlAction;
using pico_clock::DisplayPage;
using pico_clock::Ds3231;
using pico_clock::DisplaySchedule;
using pico_clock::DeviceMetrics;
using pico_clock::LedDisplay;
using pico_clock::FrontButtons;
using pico_clock::NtpClient;
using pico_clock::PowerSaveController;
using pico_clock::StatusText;
using pico_clock::LocalTime;
using pico_clock::Timezone;
using pico_clock::WifiManager;

bi_decl(bi_program_name("greenpico"));
bi_decl(bi_program_description("Pico Clock Green Wi-Fi and NTP firmware"));
bi_decl(bi_program_version_string(pico_clock::kFirmwareVersion));
bi_decl(bi_1pin_with_name(pico_clock::board::kButtonSet, "SET button"));
bi_decl(bi_1pin_with_name(pico_clock::board::kRtcSda, "DS3231 SDA"));
bi_decl(bi_1pin_with_name(pico_clock::board::kRtcScl, "DS3231 SCL"));
bi_decl(bi_1pin_with_name(pico_clock::board::kDisplayClock, "Display CLK"));
bi_decl(bi_1pin_with_name(pico_clock::board::kDisplayData, "Display SDI"));
bi_decl(bi_1pin_with_name(pico_clock::board::kDisplayLatch, "Display LE"));
bi_decl(bi_1pin_with_name(pico_clock::board::kDisplayOutputEnable, "Display OE"));
bi_decl(bi_1pin_with_name(pico_clock::board::kBuzzer, "Hourly chime buzzer"));
bi_decl(bi_1pin_with_name(pico_clock::board::kButtonDown, "DOWN button"));
bi_decl(bi_1pin_with_name(pico_clock::board::kDisplayAddress0, "Display A0"));
bi_decl(bi_1pin_with_name(pico_clock::board::kButtonUp, "UP button"));
bi_decl(bi_1pin_with_name(pico_clock::board::kDisplayAddress1, "Display A1"));
bi_decl(bi_1pin_with_name(pico_clock::board::kDisplayAddress2, "Display A2"));
bi_decl(bi_1pin_with_name(pico_clock::board::kAmbientLight, "Ambient light ADC"));

volatile bool ntp_result_ready = false;
volatile bool ntp_result_success = false;
volatile std::uint32_t ntp_epoch = 0;

void ntp_complete(bool success, std::uint32_t unix_seconds, void*) {
    ntp_epoch = unix_seconds;
    ntp_result_success = success;
    __dmb();
    ntp_result_ready = true;
}

ClockTime clock_time(const datetime_t& value) {
    return ClockTime{true, value.year, value.month, value.day, value.dotw,
                     value.hour, value.min, value.sec};
}

bool update_rtc(Ds3231& rtc, std::uint32_t epoch, const char* timezone,
                ClockTime& synchronized_time) {
    LocalTime local{};
    if (!Timezone::to_local(epoch, timezone, local)) return false;

    datetime_t value{
        local.year,
        local.month,
        local.day,
        local.weekday,
        local.hour,
        local.minute,
        local.second,
    };
    if (!rtc.write(value)) return false;
    synchronized_time = ClockTime{
        true, local.year, local.month, local.day, local.weekday,
        local.hour, local.minute, local.second,
    };
    return true;
}

void print_rtc(const Ds3231& rtc) {
    datetime_t value{};
    if (rtc.read(value)) {
        std::printf("RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                    value.year, value.month, value.day,
                    value.hour, value.min, value.sec);
    } else {
        std::printf("RTC read failed\n");
    }
}

std::uint32_t address_span(const std::uint8_t* end,
                           std::uintptr_t beginning) {
    const auto end_address = reinterpret_cast<std::uintptr_t>(end);
    if (end_address <= beginning) return 0;
    const auto size = end_address - beginning;
    return static_cast<std::uint32_t>(std::min(
        size, static_cast<std::uintptr_t>(
                  std::numeric_limits<std::uint32_t>::max())));
}

DeviceMetrics device_metrics(bool watchdog_reset) {
    const auto heap = mallinfo();
    DeviceMetrics metrics{};
    metrics.uptime_seconds = static_cast<std::uint32_t>(
        to_ms_since_boot(get_absolute_time()) / 1000u);
    metrics.cpu_frequency_hz = clock_get_hz(clk_sys);
    const auto ram_total = address_span(&__StackTop, SRAM_BASE);
    const auto ram_static = address_span(&__end__, SRAM_BASE);
    const auto heap_capacity = address_span(
        &__HeapLimit, reinterpret_cast<std::uintptr_t>(&__end__));
    metrics.memory = pico_clock::calculate_memory_metrics(
        ram_total, ram_static, heap_capacity,
        static_cast<std::uint32_t>(heap.uordblks));
    metrics.flash_total_bytes = PICO_FLASH_SIZE_BYTES;
    metrics.firmware_bytes = address_span(&__flash_binary_end, XIP_BASE);
    metrics.watchdog_reset = watchdog_reset;
    return metrics;
}

}  // namespace

int main() {
    const bool watchdog_reset = watchdog_caused_reboot();
    stdio_init_all();
    sleep_ms(250);
    std::printf("\ngreenpico starting\n");

    LedDisplay display;
    if (!display.init()) {
        std::printf("Display refresh timer failed to start\n");
    }

    FrontButtons buttons;
    buttons.init();
    Buzzer buzzer;
    buzzer.init();
    ClockControl controls;
    PowerSaveController power_save;
    adc_init();
    adc_gpio_init(pico_clock::board::kAmbientLight);
    adc_select_input(pico_clock::board::kAmbientLight - 26);

    AppConfig config{};
    ConfigStore config_store;
    if (config_store.load(config)) {
        std::printf("Loaded persistent configuration\n");
    } else {
        std::printf("No valid configuration; first-run setup required\n");
    }
    controls.configure_brightness(config.automatic_brightness,
                                  config.manual_brightness_percent);

    Ds3231 rtc;
    if (rtc.init()) print_rtc(rtc);

    // WifiManager owns the HTTP response workspace. Static storage keeps that
    // workspace out of the main/IRQ stack shared by raw lwIP callbacks.
    static WifiManager wifi;
    if (!wifi.init(config)) {
        std::printf("Network setup failed; rebooting in five seconds\n");
        sleep_ms(5000);
        watchdog_reboot(0, 0, 100);
    }

    NtpClient ntp;
    absolute_time_t next_sync = get_absolute_time();
    absolute_time_t next_display_refresh = get_absolute_time();
    absolute_time_t provisioning_until = at_the_end_of_time;
    float temperature_c = 0.0f;
    bool ntp_synchronized = false;
    ClockTime last_ntp_sync{};
    bool rtc_skew_available = false;
    std::int32_t rtc_skew_seconds = 0;
    int last_chime_year = -1;
    int last_chime_month = -1;
    int last_chime_day = -1;
    int last_chime_hour = -1;
    int last_chime_minute = -1;
    bool scheduled_on = true;

    while (true) {
        const auto now_ms = static_cast<std::uint32_t>(
            to_ms_since_boot(get_absolute_time()));
        AppConfig pending{};
        if (wifi.take_pending_config(pending)) {
            std::printf("Saving configuration\n");
            if (config_store.save(pending)) {
                std::printf("Configuration saved; rebooting\n");
                sleep_ms(500);
                watchdog_reboot(0, 0, 100);
            } else {
                std::printf("Configuration save failed\n");
            }
        }

        ControlAction api_action{};
        if (wifi.take_control_action(api_action)) {
            controls.apply(api_action, now_ms);
        }

        unsigned power_save_test_seconds = 0;
        if (wifi.take_power_save_test(power_save_test_seconds)) {
            std::printf("Power-saving test requested for %s\n",
                        power_save_test_seconds == 0
                            ? "a physical-button wake"
                            : "a timed wake");
            power_save.start_test(power_save_test_seconds, now_ms);
        }

        if (wifi.take_sync_request()) {
            std::printf("NTP synchronization requested by API\n");
            controls.apply(ControlAction::SynchronizeTime, now_ms);
        }

        buzzer.poll(now_ms);
        const auto button_events = buttons.poll(now_ms);
        controls.update_buttons(button_events.set_down, button_events.up_down,
                                button_events.down_down, now_ms);
        controls.tick(now_ms);
        const bool button_pressed = button_events.set_pressed ||
            button_events.up_pressed || button_events.down_pressed;
        const bool normal_display_enabled =
            controls.display_enabled(scheduled_on, now_ms);
        power_save.update(normal_display_enabled, button_pressed, now_ms);
        display.set_enabled(
            power_save.display_enabled(normal_display_enabled));
        wifi.set_power_saving(power_save.status(now_ms).active);
        const auto effects = controls.take_effects();
        if (effects.synchronize_time && wifi.mode() == WifiManager::Mode::Station) {
            std::printf("NTP synchronization requested\n");
            next_sync = get_absolute_time();
        }
        if (effects.start_provisioning &&
            wifi.mode() != WifiManager::Mode::Provisioning) {
            std::printf("Setup access point requested\n");
            if (wifi.force_provisioning(config)) {
                provisioning_until = make_timeout_time_ms(10 * 60 * 1000);
            }
        }
        if (effects.factory_reset) {
            std::printf("Factory reset requested\n");
            if (config_store.clear()) {
                sleep_ms(250);
                watchdog_reboot(0, 0, 100);
            } else {
                std::printf("Factory reset failed\n");
            }
        }

        if (wifi.mode() == WifiManager::Mode::Station && time_reached(next_sync)) {
            std::printf("Requesting time from %s\n", config.ntp_server);
            if (!ntp.request(config.ntp_server, ntp_complete, nullptr)) {
                next_sync = make_timeout_time_ms(5 * 60 * 1000);
            } else {
                next_sync = at_the_end_of_time;
            }
        }

        if (wifi.mode() == WifiManager::Mode::Provisioning &&
            time_reached(provisioning_until)) {
            std::printf("Temporary setup access point expired; returning to station mode\n");
            provisioning_until = at_the_end_of_time;
            wifi.force_station(config);
        }

        ntp.poll();
        if (ntp_result_ready) {
            __dmb();
            const bool success = ntp_result_success;
            const auto epoch = ntp_epoch;
            ntp_result_ready = false;
            datetime_t rtc_before{};
            const bool rtc_before_valid = rtc.read(rtc_before);
            ClockTime synchronized_time{};
            if (success && update_rtc(rtc, epoch, config.timezone,
                                      synchronized_time)) {
                std::int32_t measured_skew = 0;
                rtc_skew_available = rtc_before_valid &&
                    pico_clock::clock_time_difference_seconds(
                        clock_time(rtc_before), synchronized_time,
                        measured_skew);
                if (rtc_skew_available) rtc_skew_seconds = measured_skew;
                last_ntp_sync = synchronized_time;
                ntp_synchronized = true;
                if (rtc_skew_available) {
                    std::printf("NTP synchronization complete; RTC skew %+ld seconds\n",
                                static_cast<long>(rtc_skew_seconds));
                } else {
                    std::printf("NTP synchronization complete; prior RTC time unavailable\n");
                }
                print_rtc(rtc);
                next_sync = make_timeout_time_ms(6 * 60 * 60 * 1000);
            } else {
                ntp_synchronized = false;
                std::printf("NTP synchronization failed; retrying in five minutes\n");
                next_sync = make_timeout_time_ms(5 * 60 * 1000);
            }
        }

        if (time_reached(next_display_refresh)) {
            datetime_t value{};
            if (rtc.read(value)) {
                if (pico_clock::chime_due(config.chime_interval_minutes,
                                          value.min, value.sec) &&
                    (value.year != last_chime_year ||
                     value.month != last_chime_month ||
                     value.day != last_chime_day ||
                     value.hour != last_chime_hour ||
                     value.min != last_chime_minute)) {
                    buzzer.beep(now_ms);
                    last_chime_year = value.year;
                    last_chime_month = value.month;
                    last_chime_day = value.day;
                    last_chime_hour = value.hour;
                    last_chime_minute = value.min;
                }
                scheduled_on = DisplaySchedule::display_on(
                    config.display_schedule_enabled,
                    config.display_off_minute,
                    config.display_on_minute, value.hour, value.min);
                controls.schedule_state(scheduled_on);
                adc_select_input(pico_clock::board::kAmbientLight - 26);
                const std::uint16_t ambient_sample = adc_read();
                const std::uint8_t brightness_level =
                    controls.brightness(ambient_sample, now_ms);
                const int ambient_light_percent =
                    static_cast<int>(ambient_sample) * 100 / 4095;
                const int brightness_percent = brightness_level * 10;
                display.set_brightness(brightness_level);
                rtc.read_temperature(temperature_c);
                const bool blink_on = ((now_ms / 500u) & 1u) == 0u;
                const auto status = controls.status(now_ms);
                ClockFace::Frame frame{};
                if (status.factory_reset_countdown_seconds > 0) {
                    frame = ClockFace::render_reset_countdown(
                        status.factory_reset_countdown_seconds, blink_on);
                } else switch (status.page) {
                    case DisplayPage::Time:
                        frame = ClockFace::render_time(
                            value.hour, value.min, value.sec, value.dotw, blink_on);
                        break;
                    case DisplayPage::Date:
                        frame = ClockFace::render_date(
                            value.month, value.day, value.dotw);
                        break;
                    case DisplayPage::Temperature:
                        frame = ClockFace::render_temperature(
                            temperature_c,
                            status.temperature_unit ==
                                pico_clock::TemperatureUnit::Fahrenheit);
                        break;
                    case DisplayPage::Status: {
                        char text[96]{};
                        if (status.status_view == pico_clock::StatusView::Wifi) {
                            const bool connected =
                                wifi.mode() == WifiManager::Mode::Station;
                            StatusText::wifi(text, sizeof(text),
                                connected ? config.ssid : pico_clock::kDeviceName,
                                wifi.signal_percent(), connected);
                        } else if (status.status_view ==
                                   pico_clock::StatusView::Ntp) {
                            StatusText::ntp(text, sizeof(text), config.ntp_server,
                                           ntp_synchronized);
                        } else {
                            StatusText::brightness(
                                text, sizeof(text), ambient_light_percent,
                                brightness_percent,
                                status.automatic_brightness);
                        }
                        frame = ClockFace::render_text(
                            text, (now_ms - status.status_view_started_ms) / 300u);
                        break;
                    }
                }
                ClockFace::apply_status_indicators(
                    frame, status.automatic_brightness,
                    config.chime_interval_minutes != 0);
                display.set_frame(frame);
                wifi.update_runtime_status(status, temperature_c,
                                           ntp_synchronized,
                                           last_ntp_sync,
                                           rtc_skew_available,
                                           rtc_skew_seconds,
                                           ambient_light_percent,
                                           brightness_percent,
                                           device_metrics(watchdog_reset),
                                           clock_time(value),
                                           frame,
                                           power_save.status(now_ms));
            } else {
                display.set_frame({});
            }
            next_display_refresh = make_timeout_time_ms(200);
        }

        wifi.poll();
        sleep_ms(20);
    }
}
