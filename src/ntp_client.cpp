/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 * NTP client adapted from Raspberry Pi pico-examples.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ntp_client.hpp"

#include <array>
#include <cstring>

#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "pico/cyw43_arch.h"

namespace pico_clock {
namespace {

constexpr u16_t kNtpPort = 123;
constexpr std::size_t kPacketSize = 48;
constexpr std::uint32_t kNtpToUnixSeconds = 2208988800u;

}  // namespace

bool NtpClient::init() {
    if (pcb_ != nullptr) return true;
    pcb_ = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb_ == nullptr) return false;
    udp_recv(pcb_, receive, this);
    return true;
}

bool NtpClient::request(const char* server, Callback callback, void* context) {
    if (pending_ || server == nullptr || server[0] == '\0' || !init()) return false;
    callback_ = callback;
    callback_context_ = context;
    pending_ = true;
    deadline_ = make_timeout_time_ms(10000);

    cyw43_arch_lwip_begin();
    const err_t result = dns_gethostbyname(server, &server_address_, dns_found, this);
    if (result == ERR_OK) {
        send_request();
    } else if (result != ERR_INPROGRESS) {
        cyw43_arch_lwip_end();
        finish(false, 0);
        return false;
    }
    cyw43_arch_lwip_end();
    return true;
}

void NtpClient::poll() {
    if (pending_ && time_reached(deadline_)) finish(false, 0);
}

void NtpClient::cancel() {
    pending_ = false;
    callback_ = nullptr;
    callback_context_ = nullptr;
}

void NtpClient::dns_found(const char*, const ip_addr_t* address, void* argument) {
    auto* client = static_cast<NtpClient*>(argument);
    if (!client->pending_) return;
    if (address == nullptr) {
        client->finish(false, 0);
        return;
    }
    client->server_address_ = *address;
    client->send_request();
}

void NtpClient::send_request() {
    auto* packet = pbuf_alloc(PBUF_TRANSPORT, kPacketSize, PBUF_RAM);
    if (packet == nullptr) {
        finish(false, 0);
        return;
    }
    std::array<std::uint8_t, kPacketSize> request{};
    request[0] = 0x1b;
    pbuf_take(packet, request.data(), request.size());
    const auto result = udp_sendto(pcb_, packet, &server_address_, kNtpPort);
    pbuf_free(packet);
    if (result != ERR_OK) finish(false, 0);
}

void NtpClient::receive(void* argument, udp_pcb*, pbuf* packet,
                        const ip_addr_t* address, u16_t port) {
    auto* client = static_cast<NtpClient*>(argument);
    if (!client->pending_ || packet == nullptr) {
        if (packet != nullptr) pbuf_free(packet);
        return;
    }

    const std::uint8_t mode = pbuf_get_at(packet, 0) & 0x07;
    const std::uint8_t stratum = pbuf_get_at(packet, 1);
    const bool valid = ip_addr_cmp(address, &client->server_address_) &&
                       port == kNtpPort && packet->tot_len >= kPacketSize &&
                       mode == 4 && stratum != 0;
    std::uint32_t unix_seconds = 0;
    if (valid) {
        std::array<std::uint8_t, 4> seconds{};
        pbuf_copy_partial(packet, seconds.data(), seconds.size(), 40);
        const std::uint32_t ntp_seconds =
            (static_cast<std::uint32_t>(seconds[0]) << 24) |
            (static_cast<std::uint32_t>(seconds[1]) << 16) |
            (static_cast<std::uint32_t>(seconds[2]) << 8) |
            static_cast<std::uint32_t>(seconds[3]);
        if (ntp_seconds >= kNtpToUnixSeconds) {
            unix_seconds = ntp_seconds - kNtpToUnixSeconds;
        }
    }
    pbuf_free(packet);
    client->finish(valid && unix_seconds != 0, unix_seconds);
}

void NtpClient::finish(bool success, std::uint32_t unix_seconds) {
    if (!pending_) return;
    pending_ = false;
    const auto callback = callback_;
    void* context = callback_context_;
    callback_ = nullptr;
    callback_context_ = nullptr;
    if (callback != nullptr) callback(success, unix_seconds, context);
}

}  // namespace pico_clock
