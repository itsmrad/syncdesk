#include "core/JitterBuffer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <spdlog/spdlog.h>

namespace am::core {

JitterBuffer::JitterBuffer(JitterBufferConfig config)
    : config_(config) {
    spdlog::info("JitterBuffer created (target={}ms, min={}ms, max={}ms)",
                 config_.target_depth_ms, config_.min_depth_ms, config_.max_depth_ms);
}

JitterBuffer::~JitterBuffer() = default;

void JitterBuffer::push(AudioFrame frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Track arrival time for jitter estimation
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();

    if (last_arrival_us_ > 0) {
        const auto expected_interval_us =
            static_cast<std::int64_t>(config_.frame_size_samples) * 1'000'000 /
            static_cast<std::int64_t>(config_.sample_rate);
        const auto actual_interval_us = now - last_arrival_us_;
        const auto deviation_us = std::abs(actual_interval_us - expected_interval_us);
        const auto deviation_ms = static_cast<float>(deviation_us) / 1000.0F;

        // Exponential moving average (RFC 3550 algorithm)
        constexpr float kAlpha = 1.0F / 16.0F;
        jitter_estimate_ms_ += kAlpha * (deviation_ms - jitter_estimate_ms_);
    }
    last_arrival_us_ = now;

    // Insert in sequence order
    const auto it = std::lower_bound(buffer_.begin(), buffer_.end(), frame,
        [](const AudioFrame& a, const AudioFrame& b) {
            // Handle sequence number wrap-around (16-bit)
            const auto diff = static_cast<std::int16_t>(
                static_cast<std::int16_t>(a.sequence_number) -
                static_cast<std::int16_t>(b.sequence_number));
            return diff < 0;
        });
    buffer_.insert(it, std::move(frame));

    const auto target_depth = static_cast<std::size_t>(
        config_.target_depth_ms * config_.sample_rate /
        (1000 * config_.frame_size_samples));
    if (!primed_ && buffer_.size() >= target_depth) {
        primed_ = true;
        next_sequence_ = buffer_.front().sequence_number;
        spdlog::debug("JitterBuffer primed ({} frames buffered)", buffer_.size());
    }
}

AudioFrame JitterBuffer::pop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!primed_ || buffer_.empty()) {
        // Underrun — return silence
        AudioFrame silence;
        silence.sequence_number = next_sequence_++;
        silence.samples.resize(
            static_cast<std::size_t>(config_.frame_size_samples) * config_.channels,
            0.0F);
        return silence;
    }

    // Check if the front frame is what we expect
    if (buffer_.front().sequence_number == next_sequence_) {
        auto frame = std::move(buffer_.front());
        buffer_.pop_front();
        ++next_sequence_;
        return frame;
    }

    // Missing frame — return silence for concealment
    spdlog::trace("JitterBuffer: missing seq {}, concealing", next_sequence_);
    AudioFrame silence;
    silence.sequence_number = next_sequence_++;
    silence.samples.resize(
        static_cast<std::size_t>(config_.frame_size_samples) * config_.channels,
        0.0F);
    return silence;
}

std::size_t JitterBuffer::depth() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size();
}

float JitterBuffer::estimated_jitter_ms() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return jitter_estimate_ms_;
}

}  // namespace am::core
