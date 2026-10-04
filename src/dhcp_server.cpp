#include "dhcp_server.hpp"

#include <array>
#include <cstdint>
#include <cstring>

#include "lwip/def.h"
#include "lwip/pbuf.h"

namespace pico_clock {
namespace {

constexpr u16_t kServerPort = 67;
constexpr u16_t kClientPort = 68;
constexpr std::uint32_t kMagicCookie = 0x63825363;
constexpr std::uint8_t kDiscover = 1;
constexpr std::uint8_t kOffer = 2;
constexpr std::uint8_t kRequest = 3;
constexpr std::uint8_t kAck = 5;

#pragma pack(push, 1)
struct DhcpPacket {
    std::uint8_t operation;
    std::uint8_t hardware_type;
    std::uint8_t hardware_length;
    std::uint8_t hops;
    std::uint32_t transaction_id;
    std::uint16_t seconds;
    std::uint16_t flags;
    std::uint32_t client_address;
    std::uint32_t your_address;
    std::uint32_t server_address;
    std::uint32_t gateway_address;
    std::uint8_t client_hardware_address[16];
    std::uint8_t server_name[64];
    std::uint8_t boot_file[128];
    std::uint32_t cookie;
    std::uint8_t options[312];
};
#pragma pack(pop)

std::uint8_t message_type(const DhcpPacket& packet, std::size_t length) {
    if (length < offsetof(DhcpPacket, options) ||
        lwip_ntohl(packet.cookie) != kMagicCookie) {
        return 0;
    }

    const auto option_length = length - offsetof(DhcpPacket, options);
    for (std::size_t index = 0; index < option_length;) {
        const auto code = packet.options[index++];
        if (code == 255) break;
        if (code == 0) continue;
        if (index >= option_length) break;
        const auto size = packet.options[index++];
        if (index + size > option_length) break;
        if (code == 53 && size == 1) return packet.options[index];
        index += size;
    }
    return 0;
}

void append_option(std::uint8_t*& cursor, std::uint8_t code,
                   const void* value, std::uint8_t size) {
    *cursor++ = code;
    *cursor++ = size;
    std::memcpy(cursor, value, size);
    cursor += size;
}

}  // namespace

bool DhcpServer::start(const ip4_addr_t& gateway, const ip4_addr_t& netmask) {
    stop();
    gateway_ = gateway;
    netmask_ = netmask;
    pcb_ = udp_new_ip_type(IPADDR_TYPE_V4);
    if (pcb_ == nullptr || udp_bind(pcb_, IP_ANY_TYPE, kServerPort) != ERR_OK) {
        stop();
        return false;
    }
    udp_recv(pcb_, receive, this);
    return true;
}

void DhcpServer::stop() {
    if (pcb_ != nullptr) {
        udp_remove(pcb_);
        pcb_ = nullptr;
    }
}

void DhcpServer::receive(void* argument, udp_pcb*, pbuf* packet,
                         const ip_addr_t*, u16_t) {
    static_cast<DhcpServer*>(argument)->handle(packet);
    pbuf_free(packet);
}

void DhcpServer::handle(pbuf* packet) {
    DhcpPacket request{};
    const auto copied = pbuf_copy_partial(packet, &request,
        packet->tot_len < sizeof(request) ? packet->tot_len : sizeof(request), 0);
    const auto type = message_type(request, copied);
    if (request.operation != 1 || request.hardware_type != 1 ||
        request.hardware_length != 6 || (type != kDiscover && type != kRequest)) {
        return;
    }

    DhcpPacket response{};
    response.operation = 2;
    response.hardware_type = request.hardware_type;
    response.hardware_length = request.hardware_length;
    response.transaction_id = request.transaction_id;
    response.flags = request.flags;
    response.your_address = gateway_.addr + lwip_htonl(1);
    response.server_address = gateway_.addr;
    std::memcpy(response.client_hardware_address,
                request.client_hardware_address, sizeof(response.client_hardware_address));
    response.cookie = lwip_htonl(kMagicCookie);

    auto* cursor = response.options;
    const std::uint8_t response_type = type == kDiscover ? kOffer : kAck;
    append_option(cursor, 53, &response_type, 1);
    append_option(cursor, 54, &gateway_.addr, 4);
    const std::uint32_t lease = lwip_htonl(86400);
    append_option(cursor, 51, &lease, 4);
    append_option(cursor, 1, &netmask_.addr, 4);
    append_option(cursor, 3, &gateway_.addr, 4);
    append_option(cursor, 6, &gateway_.addr, 4);
    *cursor++ = 255;

    const auto response_size = offsetof(DhcpPacket, options) +
                               static_cast<std::size_t>(cursor - response.options);
    auto* outgoing = pbuf_alloc(PBUF_TRANSPORT, response_size, PBUF_RAM);
    if (outgoing == nullptr) return;
    pbuf_take(outgoing, &response, response_size);
    ip_addr_t broadcast;
    IP_ADDR4(&broadcast, 255, 255, 255, 255);
    udp_sendto(pcb_, outgoing, &broadcast, kClientPort);
    pbuf_free(outgoing);
}

}  // namespace pico_clock
