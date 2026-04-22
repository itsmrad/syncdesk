#include "transport/RtpSender.h"

#include <asio.hpp>
#include <cstring>
#include <spdlog/spdlog.h>
#include <vector>

namespace am::transport {

// ── RTP Header serialization ────────────────────────────────────────────

std::array<std::uint8_t, 12> RtpHeader::serialize() const noexcept {
    std::array<std::uint8_t, 12> buf{};

    buf[0] = static_cast<std::uint8_t>(
        ((version & 0x3) << 6) |
        (padding ? 0x20 : 0) |
        (extension ? 0x10 : 0) |
        (csrc_count & 0x0F));

    buf[1] = static_cast<std::uint8_t>(
        (marker ? 0x80 : 0) |
        (payload_type & 0x7F));

    buf[2] = static_cast<std::uint8_t>((sequence_number >> 8) & 0xFF);
    buf[3] = static_cast<std::uint8_t>(sequence_number & 0xFF);

    buf[4] = static_cast<std::uint8_t>((timestamp >> 24) & 0xFF);
    buf[5] = static_cast<std::uint8_t>((timestamp >> 16) & 0xFF);
    buf[6] = static_cast<std::uint8_t>((timestamp >> 8)  & 0xFF);
    buf[7] = static_cast<std::uint8_t>(timestamp & 0xFF);

    buf[8]  = static_cast<std::uint8_t>((ssrc >> 24) & 0xFF);
    buf[9]  = static_cast<std::uint8_t>((ssrc >> 16) & 0xFF);
    buf[10] = static_cast<std::uint8_t>((ssrc >> 8)  & 0xFF);
    buf[11] = static_cast<std::uint8_t>(ssrc & 0xFF);

    return buf;
}

RtpHeader RtpHeader::deserialize(std::span<const std::uint8_t, 12> data) noexcept {
    RtpHeader h;
    h.version    = (data[0] >> 6) & 0x3;
    h.padding    = (data[0] & 0x20) != 0;
    h.extension  = (data[0] & 0x10) != 0;
    h.csrc_count = data[0] & 0x0F;
    h.marker     = (data[1] & 0x80) != 0;
    h.payload_type = data[1] & 0x7F;
    h.sequence_number = static_cast<std::uint16_t>((data[2] << 8) | data[3]);
    h.timestamp = static_cast<std::uint32_t>(
        (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7]);
    h.ssrc = static_cast<std::uint32_t>(
        (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11]);
    return h;
}

// ── RtpSender (Asio UDP) ────────────────────────────────────────────────

struct RtpSender::Impl {
    asio::io_context io_context;
    asio::ip::udp::socket socket;
    asio::ip::udp::endpoint remote_endpoint;
    std::vector<std::uint8_t> send_buffer;  // Pre-allocated send buffer

    Impl(const std::string& host, std::uint16_t port)
        : socket(io_context, asio::ip::udp::v4())
        , remote_endpoint(asio::ip::make_address(host), port) {
        // Reserve for typical MTU: 12-byte header + max payload
        send_buffer.reserve(1500);
    }
};

RtpSender::RtpSender(const std::string& dest_host, std::uint16_t dest_port,
                      std::uint32_t ssrc)
    : impl_(std::make_unique<Impl>(dest_host, dest_port))
    , ssrc_(ssrc) {
    spdlog::info("RtpSender created -> {}:{} (ssrc={})", dest_host, dest_port, ssrc);
}

RtpSender::~RtpSender() = default;

bool RtpSender::send(std::span<const std::uint8_t> payload,
                     std::uint32_t timestamp) {
    RtpHeader header;
    header.ssrc = ssrc_;
    header.sequence_number = sequence_number_++;
    header.timestamp = timestamp;

    const auto header_bytes = header.serialize();

    // Reuse pre-allocated buffer — no heap allocation per packet.
    auto& packet = impl_->send_buffer;
    packet.clear();
    packet.insert(packet.end(), header_bytes.begin(), header_bytes.end());
    packet.insert(packet.end(), payload.begin(), payload.end());

    asio::error_code ec;
    impl_->socket.send_to(
        asio::buffer(packet), impl_->remote_endpoint, 0, ec);

    if (ec) {
        spdlog::error("RTP send failed: {}", ec.message());
        return false;
    }

    packets_sent_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

}  // namespace am::transport
