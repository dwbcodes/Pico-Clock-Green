#include "device_metrics.hpp"

#include <algorithm>
#include <limits>

namespace pico_clock {

MemoryMetrics calculate_memory_metrics(std::uint32_t ram_total_bytes,
                                       std::uint32_t ram_static_bytes,
                                       std::uint32_t heap_capacity_bytes,
                                       std::uint32_t heap_used_bytes) {
    MemoryMetrics result{};
    result.ram_total_bytes = ram_total_bytes;
    result.ram_static_bytes = std::min(ram_static_bytes, ram_total_bytes);
    result.heap_capacity_bytes = std::min(
        heap_capacity_bytes, ram_total_bytes - result.ram_static_bytes);
    result.heap_used_bytes = std::min(heap_used_bytes,
                                      result.heap_capacity_bytes);
    const std::uint64_t allocated =
        static_cast<std::uint64_t>(result.ram_static_bytes) +
        result.heap_used_bytes;
    result.ram_allocated_bytes = static_cast<std::uint32_t>(std::min(
        allocated, static_cast<std::uint64_t>(ram_total_bytes)));
    return result;
}

}  // namespace pico_clock
