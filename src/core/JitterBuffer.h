#pragma once

/// @file JitterBuffer.h
/// @brief Adaptive jitter buffer for network audio reception.
///
/// Absorbs variance in RTP packet arrival times. Dynamically sizes itself
/// based on observed jitter. Uses a simple circular buffer of decoded audio
/// frames.

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace am::core {

/// Configuration for the jitter buffer.
struct JitterBufferConfig {
    std::uint32_t min_depth_ms{5};    ///< Minimum buffer depth in milliseconds.
    std::uint32_t max_depth_ms{50};   ///< Maximum buffer depth in milliseconds.
    std::uint32_t target_depth_ms{10}; ///< Target buffer depth.
    std::uint32_t sample_rate{48000};
    std::uint32_t channels{2};
    std::uint32_t frame_size_samples{240}; ///< Samples per frame (5ms @ 48kHz).
};

/// A single audio frame with its RTP sequence number.
struct AudioFrame {
    std::uint16_t sequence_number{0};
    std::vector<float> samples;       ///< Interleaved PCM samples.
};

/// Adaptive jitter buffer.
///
/// Thread safety: all methods are mutex-protected. The jitter buffer sits
/// between the network decode thread and the ring buffer — it is NOT on the
/// real-time audio path (the RingBuffer handles that lock-free handoff).
class JitterBuffer {
public:
    explicit JitterBuffer(JitterBufferConfig config);
    ~JitterBuffer();

    /// Push a received frame into the buffer (called from network thread).
    void push(AudioFrame frame);

    /// Pull the next frame for playback (called from decode thread).
    /// Returns an empty frame (silence) if the buffer is underrun.
    [[nodiscard]] AudioFrame pop();

    /// Current buffer depth in frames.
    [[nodiscard]] std::size_t depth() const noexcept;

    /// Current estimated jitter in milliseconds.
    [[nodiscard]] float estimated_jitter_ms() const noexcept;

private:
    JitterBufferConfig config_;
    mutable std::mutex mutex_;
    std::deque<AudioFrame> buffer_;
    std::uint16_t next_sequence_{0};
    bool primed_{false};

    // Jitter estimation (exponential moving average)
    float jitter_estimate_ms_{0.0F};
    std::int64_t last_arrival_us_{0};
};

}  // namespace am::core
