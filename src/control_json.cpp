#include "control_json.hpp"

#include <cstring>

#include "config_json.hpp"
#include "power_save.hpp"

namespace pico_clock {
namespace {

bool action_name(const char* body, char (&output)[32]) {
    return ConfigJson::string_value(body, "action", output, sizeof(output));
}

}  // namespace

bool ControlJson::parse_display_action(const char* body, ControlAction& action) {
    char name[32]{};
    if (!action_name(body, name)) return false;
    if (std::strcmp(name, "next-page") == 0) action = ControlAction::NextPage;
    else if (std::strcmp(name, "show-time") == 0) action = ControlAction::ShowTime;
    else if (std::strcmp(name, "show-date") == 0) action = ControlAction::ShowDate;
    else if (std::strcmp(name, "show-temperature") == 0) action = ControlAction::ShowTemperature;
    else if (std::strcmp(name, "show-status") == 0) action = ControlAction::ShowStatus;
    else if (std::strcmp(name, "next-status") == 0) action = ControlAction::NextStatus;
    else if (std::strcmp(name, "previous-status") == 0) action = ControlAction::PreviousStatus;
    else if (std::strcmp(name, "brightness-up") == 0) action = ControlAction::BrightnessUp;
    else if (std::strcmp(name, "brightness-down") == 0) action = ControlAction::BrightnessDown;
    else if (std::strcmp(name, "automatic-brightness") == 0) action = ControlAction::EnableAutoBrightness;
    else if (std::strcmp(name, "select-celsius") == 0) action = ControlAction::SelectCelsius;
    else if (std::strcmp(name, "select-fahrenheit") == 0) action = ControlAction::SelectFahrenheit;
    else if (std::strcmp(name, "wake") == 0) action = ControlAction::WakeDisplay;
    else if (std::strcmp(name, "sleep-until-schedule") == 0) action = ControlAction::SleepUntilSchedule;
    else if (std::strcmp(name, "maximum-brightness") == 0) action = ControlAction::TemporaryMaximumBrightness;
    else return false;
    return true;
}

bool ControlJson::parse_power_save_test(const char* body,
                                        unsigned& duration_seconds) {
    return ConfigJson::unsigned_value(body, "durationSeconds",
                                      duration_seconds) &&
        PowerSaveController::valid_test_duration(duration_seconds);
}

bool ControlJson::confirms_factory_reset(const char* body) {
    char confirmation[24]{};
    return ConfigJson::string_value(body, "confirmation", confirmation,
                                    sizeof(confirmation)) &&
        std::strcmp(confirmation, "erase-all-settings") == 0;
}

}  // namespace pico_clock
