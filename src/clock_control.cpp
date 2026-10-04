#include "clock_control.hpp"

#include <algorithm>

namespace pico_clock {
namespace {

bool elapsed(std::uint32_t now, std::uint32_t started, std::uint32_t duration) {
    return static_cast<std::uint32_t>(now - started) >= duration;
}

}  // namespace

void ClockControl::interact(std::uint32_t now_ms) {
    page_deadline_ms_ = now_ms + kPageTimeoutMs;
    state_.display_override = DisplayOverride::OnTemporarily;
    override_deadline_ms_ = now_ms + kTemporaryWakeMs;
}

void ClockControl::apply(ControlAction action, std::uint32_t now_ms) {
    interact(now_ms);
    switch (action) {
        case ControlAction::NextPage:
            state_.page = static_cast<DisplayPage>(
                (static_cast<unsigned>(state_.page) + 1u) % 4u);
            if (state_.page == DisplayPage::Status) {
                state_.status_view = StatusView::Wifi;
                state_.status_view_started_ms = now_ms;
            }
            break;
        case ControlAction::ShowTime: state_.page = DisplayPage::Time; break;
        case ControlAction::ShowDate: state_.page = DisplayPage::Date; break;
        case ControlAction::ShowTemperature:
            state_.page = DisplayPage::Temperature;
            break;
        case ControlAction::ShowStatus:
            state_.page = DisplayPage::Status;
            state_.status_view = StatusView::Wifi;
            state_.status_view_started_ms = now_ms;
            break;
        case ControlAction::NextStatus:
            state_.status_view = static_cast<StatusView>(
                (static_cast<unsigned>(state_.status_view) + 1u) % 3u);
            state_.status_view_started_ms = now_ms;
            break;
        case ControlAction::PreviousStatus:
            state_.status_view = static_cast<StatusView>(
                (static_cast<unsigned>(state_.status_view) + 2u) % 3u);
            state_.status_view_started_ms = now_ms;
            break;
        case ControlAction::BrightnessUp:
            state_.brightness_bias = std::min<std::int8_t>(4, state_.brightness_bias + 1);
            break;
        case ControlAction::BrightnessDown:
            state_.brightness_bias = std::max<std::int8_t>(-4, state_.brightness_bias - 1);
            break;
        case ControlAction::EnableAutoBrightness:
            state_.automatic_brightness = true;
            break;
        case ControlAction::SelectCelsius:
            state_.temperature_unit = TemperatureUnit::Celsius;
            break;
        case ControlAction::SelectFahrenheit:
            state_.temperature_unit = TemperatureUnit::Fahrenheit;
            break;
        case ControlAction::WakeDisplay:
            state_.display_override = DisplayOverride::OnTemporarily;
            override_deadline_ms_ = now_ms + kTemporaryWakeMs;
            break;
        case ControlAction::SleepUntilSchedule:
            state_.display_override = DisplayOverride::OffUntilSchedule;
            break;
        case ControlAction::TemporaryMaximumBrightness:
            state_.display_override = DisplayOverride::OnTemporarily;
            override_deadline_ms_ = now_ms + kTemporaryWakeMs;
            maximum_brightness_until_ms_ = now_ms + kTemporaryWakeMs;
            break;
        case ControlAction::SynchronizeTime:
            effects_.synchronize_time = true;
            break;
        case ControlAction::StartProvisioning:
            effects_.start_provisioning = true;
            break;
        case ControlAction::FactoryReset:
            effects_.factory_reset = true;
            break;
    }
}

void ClockControl::update_buttons(bool set_pressed, bool up_pressed,
                                  bool down_pressed, std::uint32_t now_ms) {
    const bool any_pressed = set_pressed || up_pressed || down_pressed;
    if (set_pressed && !previous_set_) set_started_ms_ = now_ms;
    if (up_pressed && !previous_up_) up_started_ms_ = now_ms;
    if (down_pressed && !previous_down_) down_started_ms_ = now_ms;
    if (any_pressed && !(previous_set_ || previous_up_ || previous_down_)) {
        long_action_consumed_ = false;
        interact(now_ms);
    }

    if (set_pressed && up_pressed && down_pressed) {
        const auto held = now_ms - std::max({set_started_ms_, up_started_ms_,
                                             down_started_ms_});
        factory_reset_countdown_seconds_ = static_cast<std::uint8_t>(
            held >= kFactoryResetChordMs ? 0 :
            (kFactoryResetChordMs - held + 999) / 1000);
    } else {
        factory_reset_countdown_seconds_ = 0;
    }

    handle_long_press(now_ms);

    const bool set_released = !set_pressed && previous_set_;
    const bool up_released = !up_pressed && previous_up_;
    const bool down_released = !down_pressed && previous_down_;
    if (!long_action_consumed_) {
        handle_short_press(set_released, up_released, down_released, now_ms);
    }
    if (!any_pressed) long_action_consumed_ = false;

    previous_set_ = set_pressed;
    previous_up_ = up_pressed;
    previous_down_ = down_pressed;
}

void ClockControl::handle_short_press(bool set_released, bool up_released,
                                      bool down_released, std::uint32_t now_ms) {
    if (set_released) {
        apply(ControlAction::NextPage, now_ms);
    }
    if (up_released) {
        if (state_.page == DisplayPage::Temperature) {
            apply(ControlAction::SelectFahrenheit, now_ms);
        } else if (state_.page == DisplayPage::Status) {
            apply(ControlAction::NextStatus, now_ms);
        } else {
            apply(ControlAction::BrightnessUp, now_ms);
        }
    }
    if (down_released) {
        if (state_.page == DisplayPage::Temperature) {
            apply(ControlAction::SelectCelsius, now_ms);
        } else if (state_.page == DisplayPage::Status) {
            apply(ControlAction::PreviousStatus, now_ms);
        } else {
            apply(ControlAction::BrightnessDown, now_ms);
        }
    }
}

void ClockControl::handle_long_press(std::uint32_t now_ms) {
    if (long_action_consumed_) return;
    if (previous_set_ && previous_up_ && previous_down_ &&
        elapsed(now_ms, std::max({set_started_ms_, up_started_ms_, down_started_ms_}),
                kFactoryResetChordMs)) {
        apply(ControlAction::FactoryReset, now_ms);
        long_action_consumed_ = true;
    } else if (!previous_set_ && previous_up_ && previous_down_ &&
               elapsed(now_ms, std::max(up_started_ms_, down_started_ms_),
                       kProvisioningChordMs)) {
        apply(ControlAction::StartProvisioning, now_ms);
        long_action_consumed_ = true;
    } else if (previous_set_ && !previous_up_ && !previous_down_ &&
               elapsed(now_ms, set_started_ms_, kLongPressMs)) {
        apply(ControlAction::SynchronizeTime, now_ms);
        long_action_consumed_ = true;
    } else if (!previous_set_ && previous_up_ && !previous_down_ &&
               elapsed(now_ms, up_started_ms_, kLongPressMs)) {
        apply(ControlAction::TemporaryMaximumBrightness, now_ms);
        long_action_consumed_ = true;
    } else if (!previous_set_ && !previous_up_ && previous_down_ &&
               elapsed(now_ms, down_started_ms_, kLongPressMs)) {
        apply(state_.page == DisplayPage::Status
                  ? ControlAction::StartProvisioning
                  : ControlAction::SleepUntilSchedule,
              now_ms);
        long_action_consumed_ = true;
    }
}

void ClockControl::tick(std::uint32_t now_ms) {
    if (state_.page != DisplayPage::Time && state_.page != DisplayPage::Status &&
        static_cast<std::int32_t>(now_ms - page_deadline_ms_) >= 0) {
        state_.page = DisplayPage::Time;
    }
    if (state_.display_override == DisplayOverride::OnTemporarily &&
        static_cast<std::int32_t>(now_ms - override_deadline_ms_) >= 0) {
        state_.display_override = DisplayOverride::Automatic;
    }
}

void ClockControl::schedule_state(bool display_scheduled_on) {
    if (display_scheduled_on != previous_schedule_on_) {
        if (state_.display_override == DisplayOverride::OffUntilSchedule) {
            state_.display_override = DisplayOverride::Automatic;
        }
        previous_schedule_on_ = display_scheduled_on;
    }
}

void ClockControl::configure_brightness(bool automatic,
                                        std::uint8_t manual_percent) {
    state_.automatic_brightness = automatic;
    state_.manual_brightness_percent = static_cast<std::uint8_t>(
        std::max(10, std::min(100, static_cast<int>(manual_percent))));
}

ControlStatus ClockControl::status(std::uint32_t now_ms) const {
    (void)now_ms;
    auto result = state_;
    result.factory_reset_countdown_seconds = factory_reset_countdown_seconds_;
    return result;
}

ControlEffects ClockControl::take_effects() {
    const auto result = effects_;
    effects_ = {};
    return result;
}

bool ClockControl::display_enabled(bool scheduled_on, std::uint32_t now_ms) const {
    if (state_.display_override == DisplayOverride::OffUntilSchedule) return false;
    if (state_.display_override == DisplayOverride::OnTemporarily &&
        static_cast<std::int32_t>(now_ms - override_deadline_ms_) < 0) return true;
    return scheduled_on;
}

std::uint8_t ClockControl::brightness(std::uint16_t ambient_sample,
                                      std::uint32_t now_ms) const {
    if (static_cast<std::int32_t>(now_ms - maximum_brightness_until_ms_) < 0) {
        return 10;
    }
    int level = state_.automatic_brightness
        ? 1 + static_cast<int>(ambient_sample) * 9 / 4095
        : (static_cast<int>(state_.manual_brightness_percent) + 5) / 10;
    level += state_.brightness_bias;
    return static_cast<std::uint8_t>(std::max(1, std::min(10, level)));
}

const char* ClockControl::page_name(DisplayPage page) {
    switch (page) {
        case DisplayPage::Time: return "time";
        case DisplayPage::Date: return "date";
        case DisplayPage::Temperature: return "temperature";
        case DisplayPage::Status: return "status";
    }
    return "time";
}

const char* ClockControl::override_name(DisplayOverride value) {
    switch (value) {
        case DisplayOverride::Automatic: return "automatic";
        case DisplayOverride::OnTemporarily: return "on-temporarily";
        case DisplayOverride::OffUntilSchedule: return "off-until-schedule";
    }
    return "automatic";
}

const char* ClockControl::unit_name(TemperatureUnit unit) {
    return unit == TemperatureUnit::Celsius ? "celsius" : "fahrenheit";
}

const char* ClockControl::status_view_name(StatusView view) {
    switch (view) {
        case StatusView::Wifi: return "wifi";
        case StatusView::Ntp: return "ntp";
        case StatusView::Brightness: return "brightness";
    }
    return "wifi";
}

}  // namespace pico_clock
