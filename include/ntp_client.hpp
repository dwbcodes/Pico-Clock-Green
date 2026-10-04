#pragma once

#include <cstdint>

#include "lwip/ip_addr.h"
#include "lwip/udp.h"
#include "pico/time.h"

namespace pico_clock {

class NtpClient {
public:
    using Callback = void (*)(bool success, std::uint32_t unix_seconds, void* context);

    bool init();
    bool request(const char* server, Callback callback, void* context);
    void poll();
    void cancel();

private:
    static void dns_found(const char* hostname, const ip_addr_t* address, void* argument);
    static void receive(void* argument, udp_pcb* pcb, pbuf* packet,
                        const ip_addr_t* address, u16_t port);
    void send_request();
    void finish(bool success, std::uint32_t unix_seconds);

    udp_pcb* pcb_{nullptr};
    ip_addr_t server_address_{};
    Callback callback_{nullptr};
    void* callback_context_{nullptr};
    bool pending_{false};
    absolute_time_t deadline_{};
};

}  // namespace pico_clock
