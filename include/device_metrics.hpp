#pragma once

#include <cstdint>

namespace pico_clock {

struct MemoryMetrics {
    std::uint32_t ram_total_bytes{0};
    std::uint32_t ram_static_bytes{0};
    std::uint32_t ram_allocated_bytes{0};
    std::uint32_t heap_used_bytes{0};
    std::uint32_t heap_capacity_bytes{0};
};

struct DeviceMetrics {
    std::uint32_t uptime_seconds{0};
    std::uint32_t cpu_frequency_hz{0};
    MemoryMetrics memory{};
    std::uint32_t flash_total_bytes{0};
    std::uint32_t firmware_bytes{0};
    bool watchdog_reset{false};
};

MemoryMetrics calculate_memory_metrics(std::uint32_t ram_total_bytes,
                                       std::uint32_t ram_static_bytes,
                                       std::uint32_t heap_capacity_bytes,
                                       std::uint32_t heap_used_bytes);

}  // namespace pico_clock
