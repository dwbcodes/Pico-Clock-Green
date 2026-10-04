#pragma once

#include "lwip/ip4_addr.h"
#include "lwip/udp.h"

namespace pico_clock {

class DhcpServer {
public:
    bool start(const ip4_addr_t& gateway, const ip4_addr_t& netmask);
    void stop();

private:
    static void receive(void* argument, udp_pcb* pcb, pbuf* packet,
                        const ip_addr_t* address, u16_t port);
    void handle(pbuf* packet);

    udp_pcb* pcb_{nullptr};
    ip4_addr_t gateway_{};
    ip4_addr_t netmask_{};
};

}  // namespace pico_clock
