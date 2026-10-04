#include <unity.h>

#include "number_format.hpp"

namespace {

void test_formats_positive_fixed_point_without_float_printf() {
    char output[16]{};
    TEST_ASSERT_TRUE(pico_clock::NumberFormat::fixed_two(23.625f, output,
                                                         sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("23.63", output);
}

void test_formats_negative_and_zero_fixed_point_values() {
    char output[16]{};
    TEST_ASSERT_TRUE(pico_clock::NumberFormat::fixed_two(-4.125f, output,
                                                         sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("-4.13", output);
    TEST_ASSERT_TRUE(pico_clock::NumberFormat::fixed_two(0.0f, output,
                                                         sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("0.00", output);
}

void test_rejects_a_too_small_output_buffer() {
    char output[4]{};
    TEST_ASSERT_FALSE(pico_clock::NumberFormat::fixed_two(100.0f, output,
                                                          sizeof(output)));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_formats_positive_fixed_point_without_float_printf);
    RUN_TEST(test_formats_negative_and_zero_fixed_point_values);
    RUN_TEST(test_rejects_a_too_small_output_buffer);
    return UNITY_END();
}
