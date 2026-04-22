#pragma once

/// @file RtpSender.h
/// @brief Minimal RTP sender over UDP using standalone Asio.
///
/// Implements RFC 3550 RTP header (12 bytes) — no RTCP, no DTLS, no WebRTC.
/// Just raw UDP with sequence numbers and timestamps.

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace am::transport {

/// RTP header (RFC 3550, 12 bytes fixed).
struct RtpHeader {
    std::uint8_t  version{2};
    bool          padding{false};
    bool          extension{false};
    std::uint8_t  csrc_count{0};
    bool          marker{false};
    std::uint8_t  payload_type{111};  ///< Dynamic PT for Opus
    std::uint16_t sequence_number{0};
    std::uint32_t timestamp{0};
    std::uint32_t ssrc{0};

    /// Serialize to 12-byte wire format (big-endian).
    [[nodiscard]] std::array<std::uint8_t, 12> serialize() const noexcept;

    /// Deserialize from 12-byte wire format.
    static RtpHeader deserialize(std::span<const std::uint8_t, 12> data) noexcept;
};

/// Sends RTP packets over UDP.
class RtpSender {
public:
    RtpSender(const std::string& dest_host, std::uint16_t dest_port,
              std::uint32_t ssrc);
    ~RtpSender();

    // Non-copyable
    RtpSender(const RtpSender&) = delete;
    RtpSender& operator=(const RtpSender&) = delete;

    /// Send an encoded audio payload as an RTP packet.
    /// @param payload   Opus-encoded audio data.
    /// @param timestamp RTP timestamp (sample count).
    /// @return true if the packet was sent successfully.
    [[nodiscard]] bool send(std::span<const std::uint8_t> payload,
                            std::uint32_t timestamp);

    /// Number of packets sent since creation.
    [[nodiscard]] std::uint64_t packets_sent() const noexcept {
        return packets_sent_.load(std::memory_order_relaxed);
    }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::uint32_t ssrc_;
    std::uint16_t sequence_number_{0};
    std::atomic<std::uint64_t> packets_sent_{0};
};

}  // namespace am::transport
