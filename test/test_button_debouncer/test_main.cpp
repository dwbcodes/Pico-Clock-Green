#include <unity.h>

#include "button_debouncer.hpp"

using pico_clock::ButtonDebouncer;
using pico_clock::ButtonEvent;

namespace {

void test_button_rejects_bounce_and_emits_one_press() {
    ButtonDebouncer button;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                          static_cast<int>(button.update(true, 10)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                          static_cast<int>(button.update(false, 20)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                          static_cast<int>(button.update(true, 30)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                          static_cast<int>(button.update(true, 69)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::Pressed),
                          static_cast<int>(button.update(true, 70)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                          static_cast<int>(button.update(true, 100)));
}

void test_button_emits_release_after_debounce() {
    ButtonDebouncer button;
    button.update(true, 0);
    button.update(true, 40);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                          static_cast<int>(button.update(false, 50)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::Released),
                          static_cast<int>(button.update(false, 90)));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_button_rejects_bounce_and_emits_one_press);
    RUN_TEST(test_button_emits_release_after_debounce);
    return UNITY_END();
}
