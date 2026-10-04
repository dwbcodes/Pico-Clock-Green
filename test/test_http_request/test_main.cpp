#include <unity.h>

#include "http_request.hpp"

using namespace pico_clock;

namespace {

void test_matches_exact_method_and_path() {
    TEST_ASSERT_TRUE(HttpRequest::matches(
        "GET /api/v1/status HTTP/1.1\r\nHost: greenpico\r\n\r\n",
        "GET", "/api/v1/status"));
    TEST_ASSERT_TRUE(HttpRequest::matches(
        "POST /api/v1/power-saving/test HTTP/1.1\r\n\r\n{}",
        "POST", "/api/v1/power-saving/test"));
    TEST_ASSERT_TRUE(HttpRequest::matches(
        "GET /docs HTTP/1.1\r\nHost: greenpico.internal\r\n\r\n",
        "GET", "/docs"));
    TEST_ASSERT_TRUE(HttpRequest::matches(
        "GET /docs/ HTTP/1.1\r\nHost: greenpico.internal\r\n\r\n",
        "GET", "/docs/"));
}

void test_rejects_prefixes_wrong_methods_and_queries() {
    TEST_ASSERT_FALSE(HttpRequest::matches(
        "GET /api/v1/status-extra HTTP/1.1\r\n\r\n",
        "GET", "/api/v1/status"));
    TEST_ASSERT_FALSE(HttpRequest::matches(
        "POST /api/v1/status HTTP/1.1\r\n\r\n",
        "GET", "/api/v1/status"));
    TEST_ASSERT_FALSE(HttpRequest::matches(
        "GET /api/v1/status?full=true HTTP/1.1\r\n\r\n",
        "GET", "/api/v1/status"));
}

void test_rejects_invalid_arguments() {
    TEST_ASSERT_FALSE(HttpRequest::matches(nullptr, "GET", "/"));
    TEST_ASSERT_FALSE(HttpRequest::matches("GET / HTTP/1.1", "", "/"));
    TEST_ASSERT_FALSE(HttpRequest::matches("GET / HTTP/1.1", "GET", "bad"));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_matches_exact_method_and_path);
    RUN_TEST(test_rejects_prefixes_wrong_methods_and_queries);
    RUN_TEST(test_rejects_invalid_arguments);
    return UNITY_END();
}
