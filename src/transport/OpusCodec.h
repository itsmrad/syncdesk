#pragma once

/// @file OpusCodec.h
/// @brief Wraps libopus for encoding/decoding audio frames.
///
/// Opus at 2.5ms frame size achieves ~3-5ms encoding latency.
/// Configured for low-latency real-time audio over LAN.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

struct OpusEncoder;
struct OpusDecoder;

namespace am::transport {

/// Opus codec configuration.
struct OpusConfig {
    std::int32_t sample_rate{48000};
    int channels{2};
    int bitrate{128000};           ///< Bits per second.
    int frame_size_samples{120};   ///< Samples per channel per frame (2.5ms @ 48kHz).
};

/// RAII wrapper for Opus encoder + decoder pair.
class OpusCodec {
public:
    explicit OpusCodec(OpusConfig config);
    ~OpusCodec();

    // Non-copyable, movable
    OpusCodec(const OpusCodec&) = delete;
    OpusCodec& operator=(const OpusCodec&) = delete;
    OpusCodec(OpusCodec&&) noexcept;
    OpusCodec& operator=(OpusCodec&&) noexcept;

    /// Encode interleaved float PCM samples to Opus.
    /// @param pcm       Input PCM samples (frame_size * channels floats).
    /// @param output    Output buffer for encoded data.
    /// @return Number of bytes written to output, or negative on error.
    [[nodiscard]] int encode(std::span<const float> pcm,
                             std::span<std::uint8_t> output);

    /// Decode Opus packet to interleaved float PCM.
    /// @param data      Encoded Opus packet.
    /// @param output    Output buffer for decoded PCM.
    /// @return Number of samples per channel decoded, or negative on error.
    [[nodiscard]] int decode(std::span<const std::uint8_t> data,
                             std::span<float> output);

    /// Decode with packet loss concealment (no data available).
    [[nodiscard]] int decode_plc(std::span<float> output);

    [[nodiscard]] const OpusConfig& config() const noexcept { return config_; }

private:
    OpusConfig config_;
    OpusEncoder* encoder_{nullptr};
    OpusDecoder* decoder_{nullptr};
};

}  // namespace am::transport
