#include <unity.h>

#include "device_metrics.hpp"

using pico_clock::calculate_memory_metrics;

namespace {

void test_combines_static_and_heap_allocations() {
    const auto metrics = calculate_memory_metrics(270336, 74472, 187672, 8192);
    TEST_ASSERT_EQUAL_UINT32(270336, metrics.ram_total_bytes);
    TEST_ASSERT_EQUAL_UINT32(74472, metrics.ram_static_bytes);
    TEST_ASSERT_EQUAL_UINT32(82664, metrics.ram_allocated_bytes);
    TEST_ASSERT_EQUAL_UINT32(8192, metrics.heap_used_bytes);
    TEST_ASSERT_EQUAL_UINT32(187672, metrics.heap_capacity_bytes);
}

void test_rejects_impossible_memory_values() {
    const auto metrics = calculate_memory_metrics(100, 120, 50, 75);
    TEST_ASSERT_EQUAL_UINT32(100, metrics.ram_static_bytes);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.heap_capacity_bytes);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.heap_used_bytes);
    TEST_ASSERT_EQUAL_UINT32(100, metrics.ram_allocated_bytes);
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_combines_static_and_heap_allocations);
    RUN_TEST(test_rejects_impossible_memory_values);
    return UNITY_END();
}
