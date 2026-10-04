#include <cstring>

#include <unity.h>

#include "config_json.hpp"

using pico_clock::AppConfig;
using pico_clock::ConfigJson;

namespace {

AppConfig existing_config() {
    AppConfig config{};
    std::strcpy(config.ssid, "Old network");
    std::strcpy(config.password, "old-secret");
    return config;
}

constexpr const char* kValid =
    "{\"ssid\":\"New network\",\"hostname\":\"workshop-clock\","
    "\"domainName\":\"lab.internal\",\"ntpServer\":\"pool.ntp.org\","
    "\"timezone\":\"America/Los_Angeles\","
    "\"displayScheduleEnabled\":true,\"displayOff\":\"22:00\","
    "\"displayOn\":\"07:00\"}";

void test_parses_complete_config_and_preserves_absent_password() {
    AppConfig updated{};
    TEST_ASSERT_TRUE(ConfigJson::parse(kValid, existing_config(), updated));
    TEST_ASSERT_EQUAL_STRING("New network", updated.ssid);
    TEST_ASSERT_EQUAL_STRING("workshop-clock", updated.hostname);
    TEST_ASSERT_EQUAL_STRING("lab.internal", updated.domain_name);
    TEST_ASSERT_EQUAL_STRING("old-secret", updated.password);
    TEST_ASSERT_EQUAL_STRING("America/Los_Angeles", updated.timezone);
    TEST_ASSERT_TRUE(updated.display_schedule_enabled);
    TEST_ASSERT_EQUAL(1320, updated.display_off_minute);
    TEST_ASSERT_EQUAL(420, updated.display_on_minute);
}

void test_validates_hostname_and_domain_name() {
    AppConfig updated{};
    const char* invalid_hostname =
        "{\"ssid\":\"N\",\"hostname\":\"bad.name\","
        "\"ntpServer\":\"n\",\"timezone\":\"UTC\","
        "\"displayScheduleEnabled\":false,\"displayOff\":\"22:00\","
        "\"displayOn\":\"07:00\"}";
    TEST_ASSERT_FALSE(ConfigJson::parse(
        invalid_hostname, existing_config(), updated));

    const char* invalid_domain =
        "{\"ssid\":\"N\",\"hostname\":\"clock\","
        "\"domainName\":\"bad_domain\",\"ntpServer\":\"n\","
        "\"timezone\":\"UTC\",\"displayScheduleEnabled\":false,"
        "\"displayOff\":\"22:00\",\"displayOn\":\"07:00\"}";
    TEST_ASSERT_FALSE(ConfigJson::parse(
        invalid_domain, existing_config(), updated));

    const char* empty_domain =
        "{\"ssid\":\"N\",\"hostname\":\"clock\",\"domainName\":\"\","
        "\"ntpServer\":\"n\",\"timezone\":\"UTC\","
        "\"displayScheduleEnabled\":false,\"displayOff\":\"22:00\","
        "\"displayOn\":\"07:00\"}";
    TEST_ASSERT_TRUE(ConfigJson::parse(empty_domain, existing_config(), updated));
    TEST_ASSERT_EQUAL_STRING("clock", updated.hostname);
    TEST_ASSERT_EQUAL_STRING("", updated.domain_name);
}

void test_updates_or_clears_password_explicitly() {
    AppConfig updated{};
    const char* replacement =
        "{\"ssid\":\"N\",\"password\":\"new-secret\","
        "\"ntpServer\":\"n\",\"timezone\":\"UTC\","
        "\"displayScheduleEnabled\":false,\"displayOff\":\"22:00\","
        "\"displayOn\":\"07:00\"}";
    TEST_ASSERT_TRUE(ConfigJson::parse(replacement, existing_config(), updated));
    TEST_ASSERT_EQUAL_STRING("new-secret", updated.password);

    const char* clear =
        "{\"ssid\":\"N\",\"clearPassword\":true,"
        "\"ntpServer\":\"n\",\"timezone\":\"UTC\","
        "\"displayScheduleEnabled\":false,\"displayOff\":\"22:00\","
        "\"displayOn\":\"07:00\"}";
    TEST_ASSERT_TRUE(ConfigJson::parse(clear, existing_config(), updated));
    TEST_ASSERT_EQUAL_STRING("", updated.password);
}

void test_rejects_unknown_timezone_and_ambiguous_schedule() {
    AppConfig updated{};
    const char* zone =
        "{\"ssid\":\"N\",\"ntpServer\":\"n\",\"timezone\":\"PST\","
        "\"displayScheduleEnabled\":false,\"displayOff\":\"22:00\","
        "\"displayOn\":\"07:00\"}";
    TEST_ASSERT_FALSE(ConfigJson::parse(zone, existing_config(), updated));
    const char* equal =
        "{\"ssid\":\"N\",\"ntpServer\":\"n\",\"timezone\":\"UTC\","
        "\"displayScheduleEnabled\":true,\"displayOff\":\"08:00\","
        "\"displayOn\":\"08:00\"}";
    TEST_ASSERT_FALSE(ConfigJson::parse(equal, existing_config(), updated));
}

void test_string_parser_handles_escapes_and_rejects_overflow() {
    char value[16]{};
    TEST_ASSERT_TRUE(ConfigJson::string_value("{\"ssid\":\"A\\\"B\"}",
                                              "ssid", value, sizeof(value)));
    TEST_ASSERT_EQUAL_STRING("A\"B", value);
    char small[3]{};
    TEST_ASSERT_FALSE(ConfigJson::string_value("{\"ssid\":\"long\"}",
                                               "ssid", small, sizeof(small)));
}

void test_escape_produces_valid_json_string_content() {
    char escaped[32]{};
    TEST_ASSERT_TRUE(ConfigJson::escape("A\"B\\C\n", escaped, sizeof(escaped)));
    TEST_ASSERT_EQUAL_STRING("A\\\"B\\\\C\\n", escaped);
}

void test_parses_brightness_configuration_when_present() {
    const char* body =
        "{\"ssid\":\"N\",\"ntpServer\":\"n\",\"timezone\":\"UTC\","
        "\"displayScheduleEnabled\":false,\"displayOff\":\"22:00\","
        "\"displayOn\":\"07:00\",\"automaticBrightness\":false,"
        "\"manualBrightnessPercent\":70}";
    AppConfig updated{};
    TEST_ASSERT_TRUE(ConfigJson::parse(body, existing_config(), updated));
    TEST_ASSERT_FALSE(updated.automatic_brightness);
    TEST_ASSERT_EQUAL_UINT8(70, updated.manual_brightness_percent);

    const char* invalid =
        "{\"ssid\":\"N\",\"ntpServer\":\"n\",\"timezone\":\"UTC\","
        "\"displayScheduleEnabled\":false,\"displayOff\":\"22:00\","
        "\"displayOn\":\"07:00\",\"manualBrightnessPercent\":101}";
    TEST_ASSERT_FALSE(ConfigJson::parse(invalid, existing_config(), updated));

    const char* invalid_step =
        "{\"ssid\":\"N\",\"ntpServer\":\"n\",\"timezone\":\"UTC\","
        "\"displayScheduleEnabled\":false,\"displayOff\":\"22:00\","
        "\"displayOn\":\"07:00\",\"manualBrightnessPercent\":55}";
    TEST_ASSERT_FALSE(ConfigJson::parse(invalid_step, existing_config(), updated));
}

void test_parses_and_validates_chime_interval() {
    const char* body =
        "{\"ssid\":\"N\",\"ntpServer\":\"n\",\"timezone\":\"UTC\","
        "\"displayScheduleEnabled\":false,\"displayOff\":\"22:00\","
        "\"displayOn\":\"07:00\",\"chimeInterval\":\"30min\"}";
    AppConfig updated{};
    TEST_ASSERT_TRUE(ConfigJson::parse(body, existing_config(), updated));
    TEST_ASSERT_EQUAL_UINT8(30, updated.chime_interval_minutes);

    const char* invalid =
        "{\"ssid\":\"N\",\"ntpServer\":\"n\",\"timezone\":\"UTC\","
        "\"displayScheduleEnabled\":false,\"displayOff\":\"22:00\","
        "\"displayOn\":\"07:00\",\"chimeInterval\":\"10min\"}";
    TEST_ASSERT_FALSE(ConfigJson::parse(invalid, existing_config(), updated));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_complete_config_and_preserves_absent_password);
    RUN_TEST(test_updates_or_clears_password_explicitly);
    RUN_TEST(test_validates_hostname_and_domain_name);
    RUN_TEST(test_rejects_unknown_timezone_and_ambiguous_schedule);
    RUN_TEST(test_string_parser_handles_escapes_and_rejects_overflow);
    RUN_TEST(test_escape_produces_valid_json_string_content);
    RUN_TEST(test_parses_brightness_configuration_when_present);
    RUN_TEST(test_parses_and_validates_chime_interval);
    return UNITY_END();
}
