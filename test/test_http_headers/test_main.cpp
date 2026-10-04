#include <cstring>

#include <unity.h>

#include "http_headers.hpp"

namespace {

void test_content_length_is_decimal_on_pico_compatible_printf() {
    char header[320]{};
    const int length = pico_clock::HttpHeaders::format(
        header, sizeof(header), "200 OK", "application/json", 179199,
        false, "no-store");

    TEST_ASSERT_GREATER_THAN(0, length);
    TEST_ASSERT_NOT_NULL(std::strstr(header, "Content-Length: 179199\r\n"));
    TEST_ASSERT_NULL(std::strstr(header, "Content-Length: zu"));
}

void test_gzip_header_includes_encoding_and_cache_policy() {
    char header[320]{};
    pico_clock::HttpHeaders::format(
        header, sizeof(header), "200 OK", "text/html; charset=utf-8", 42,
        true, "no-store");

    TEST_ASSERT_NOT_NULL(std::strstr(header, "Content-Encoding: gzip\r\n"));
    TEST_ASSERT_NOT_NULL(std::strstr(header, "Content-Length: 42\r\n"));
    TEST_ASSERT_NOT_NULL(std::strstr(header, "Cache-Control: no-store\r\n"));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_content_length_is_decimal_on_pico_compatible_printf);
    RUN_TEST(test_gzip_header_includes_encoding_and_cache_policy);
    return UNITY_END();
}
