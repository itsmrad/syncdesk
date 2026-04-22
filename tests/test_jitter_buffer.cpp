#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/JitterBuffer.h"

#include <vector>

using namespace am::core;

namespace {

AudioFrame make_frame(std::uint16_t seq, std::size_t sample_count, float value = 0.0F) {
    AudioFrame frame;
    frame.sequence_number = seq;
    frame.samples.assign(sample_count, value);
    return frame;
}

JitterBufferConfig default_config() {
    JitterBufferConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    config.frame_size_samples = 240;
    config.target_depth_ms = 10;
    config.min_depth_ms = 5;
    config.max_depth_ms = 50;
    return config;
}

}  // namespace

TEST_CASE("JitterBuffer: pop from empty returns silence", "[jitter]") {
    JitterBuffer jb(default_config());
    REQUIRE(jb.depth() == 0);

    const auto frame = jb.pop();
    // Should be silence-filled
    const auto expected_size = static_cast<std::size_t>(240 * 2);
    REQUIRE(frame.samples.size() == expected_size);
    for (const auto& s : frame.samples) {
        REQUIRE(s == Catch::Approx(0.0F));
    }
}

TEST_CASE("JitterBuffer: push and pop in-order", "[jitter]") {
    auto config = default_config();
    // Set target to 1 frame so it primes immediately
    config.target_depth_ms = 5;
    JitterBuffer jb(config);

    const auto sample_count = static_cast<std::size_t>(
        config.frame_size_samples * config.channels);

    // Push enough frames to prime the buffer (target = 5ms = 1 frame at 240 samples)
    jb.push(make_frame(0, sample_count, 1.0F));
    REQUIRE(jb.depth() == 1);

    const auto frame = jb.pop();
    REQUIRE(frame.sequence_number == 0);
    REQUIRE(frame.samples[0] == Catch::Approx(1.0F));
}

TEST_CASE("JitterBuffer: jitter estimation starts at zero", "[jitter]") {
    JitterBuffer jb(default_config());
    REQUIRE(jb.estimated_jitter_ms() == Catch::Approx(0.0F));
}

TEST_CASE("JitterBuffer: depth tracks push/pop", "[jitter]") {
    auto config = default_config();
    config.target_depth_ms = 5;
    JitterBuffer jb(config);

    const auto sample_count = static_cast<std::size_t>(
        config.frame_size_samples * config.channels);

    jb.push(make_frame(0, sample_count));
    jb.push(make_frame(1, sample_count));
    REQUIRE(jb.depth() == 2);

    [[maybe_unused]] auto _ = jb.pop();
    REQUIRE(jb.depth() == 1);
}

TEST_CASE("JitterBuffer: out-of-order insertion sorts by sequence", "[jitter]") {
    auto config = default_config();
    // Target depth of 3 frames so priming happens after all 3 are pushed
    config.target_depth_ms = 15;  // 15ms = 3 frames at 240 samples/frame @ 48kHz
    JitterBuffer jb(config);

    const auto sample_count = static_cast<std::size_t>(
        config.frame_size_samples * config.channels);

    // Push out of order — buffer sorts by sequence number
    jb.push(make_frame(2, sample_count, 2.0F));
    jb.push(make_frame(0, sample_count, 0.0F));
    jb.push(make_frame(1, sample_count, 1.0F));

    // Buffer should now be primed with all 3 frames sorted
    // next_sequence_ is set to front of sorted deque = 0
    auto f0 = jb.pop();
    REQUIRE(f0.sequence_number == 0);
    auto f1 = jb.pop();
    REQUIRE(f1.sequence_number == 1);
    auto f2 = jb.pop();
    REQUIRE(f2.sequence_number == 2);
}
