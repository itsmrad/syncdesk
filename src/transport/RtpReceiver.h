#pragma once

/// @file RtpReceiver.h
/// @brief Receives RTP packets over UDP and feeds them to the jitter buffer.

#include "transport/RtpSender.h"  // RtpHeader

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>

namespace am::transport {

/// Callback invoked when an RTP packet is received.
/// @param header   Parsed RTP header.
/// @param payload  Opus-encoded audio payload.
using RtpPacketCallback = std::function<void(const RtpHeader& header,
                                              std::span<const std::uint8_t> payload)>;

/// Asynchronously receives RTP packets over UDP.
class RtpReceiver {
public:
    /// @param bind_port  Local port to listen on.
    explicit RtpReceiver(std::uint16_t bind_port);
    ~RtpReceiver();

    // Non-copyable
    RtpReceiver(const RtpReceiver&) = delete;
    RtpReceiver& operator=(const RtpReceiver&) = delete;

    /// Set callback for received packets.
    void on_packet(RtpPacketCallback callback);

    /// Start receiving (runs the Asio event loop on a background thread).
    void start();

    /// Stop receiving.
    void stop();

    /// Number of packets received since creation.
    [[nodiscard]] std::uint64_t packets_received() const noexcept {
        return packets_received_.load(std::memory_order_relaxed);
    }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::mutex callback_mutex_;
    RtpPacketCallback callback_;
    std::atomic<std::uint64_t> packets_received_{0};

    void do_receive();
};

}  // namespace am::transport
