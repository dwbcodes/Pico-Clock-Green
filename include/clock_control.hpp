#pragma once

#include <cstdint>

namespace pico_clock {

enum class DisplayPage : std::uint8_t {
    Time,
    Date,
    Temperature,
    Status,
};

enum class StatusView : std::uint8_t { Wifi, Ntp, Brightness };

enum class DisplayOverride : std::uint8_t {
    Automatic,
    OnTemporarily,
    OffUntilSchedule,
};

enum class TemperatureUnit : std::uint8_t { Celsius, Fahrenheit };

enum class ControlAction : std::uint8_t {
    NextPage,
    ShowTime,
    ShowDate,
    ShowTemperature,
    ShowStatus,
    NextStatus,
    PreviousStatus,
    BrightnessUp,
    BrightnessDown,
    EnableAutoBrightness,
    SelectCelsius,
    SelectFahrenheit,
    WakeDisplay,
    SleepUntilSchedule,
    TemporaryMaximumBrightness,
    SynchronizeTime,
    StartProvisioning,
    FactoryReset,
};

struct ControlEffects {
    bool synchronize_time{false};
    bool start_provisioning{false};
    bool factory_reset{false};
};

struct ControlStatus {
    DisplayPage page{DisplayPage::Time};
    DisplayOverride display_override{DisplayOverride::Automatic};
    TemperatureUnit temperature_unit{TemperatureUnit::Fahrenheit};
    StatusView status_view{StatusView::Wifi};
    std::int8_t brightness_bias{0};
    bool automatic_brightness{true};
    std::uint8_t manual_brightness_percent{50};
    std::uint8_t factory_reset_countdown_seconds{0};
    std::uint32_t status_view_started_ms{0};
};

class ClockControl {
public:
    void apply(ControlAction action, std::uint32_t now_ms);
    void update_buttons(bool set_pressed, bool up_pressed, bool down_pressed,
                        std::uint32_t now_ms);
    void tick(std::uint32_t now_ms);
    void schedule_state(bool display_scheduled_on);
    void configure_brightness(bool automatic, std::uint8_t manual_percent);
    ControlStatus status(std::uint32_t now_ms) const;
    ControlEffects take_effects();
    bool display_enabled(bool display_scheduled_on, std::uint32_t now_ms) const;
    std::uint8_t brightness(std::uint16_t ambient_sample,
                            std::uint32_t now_ms) const;

    static const char* page_name(DisplayPage page);
    static const char* override_name(DisplayOverride value);
    static const char* unit_name(TemperatureUnit unit);
    static const char* status_view_name(StatusView view);

private:
    static constexpr std::uint32_t kPageTimeoutMs = 8000;
    static constexpr std::uint32_t kLongPressMs = 2000;
    static constexpr std::uint32_t kProvisioningChordMs = 5000;
    static constexpr std::uint32_t kFactoryResetChordMs = 10000;
    static constexpr std::uint32_t kTemporaryWakeMs = 30000;

    void interact(std::uint32_t now_ms);
    void handle_short_press(bool set_released, bool up_released,
                            bool down_released, std::uint32_t now_ms);
    void handle_long_press(std::uint32_t now_ms);

    ControlStatus state_{};
    ControlEffects effects_{};
    std::uint32_t page_deadline_ms_{0};
    std::uint32_t override_deadline_ms_{0};
    std::uint32_t maximum_brightness_until_ms_{0};
    std::uint32_t set_started_ms_{0};
    std::uint32_t up_started_ms_{0};
    std::uint32_t down_started_ms_{0};
    bool previous_set_{false};
    bool previous_up_{false};
    bool previous_down_{false};
    bool long_action_consumed_{false};
    bool previous_schedule_on_{true};
    std::uint8_t factory_reset_countdown_seconds_{0};
};

}  // namespace pico_clock
