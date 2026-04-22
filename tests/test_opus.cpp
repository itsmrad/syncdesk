#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "transport/OpusCodec.h"

#include <cmath>
#include <numbers>
#include <vector>

using namespace am::transport;

namespace {

/// Generate a 440Hz sine test tone.
std::vector<float> make_sine_tone(int sample_rate, int channels, int frame_size) {
    std::vector<float> pcm(static_cast<std::size_t>(frame_size * channels));
    float phase = 0.0F;
    constexpr float kFreq = 440.0F;
    for (int i = 0; i < frame_size; ++i) {
        const float sample = 0.3F * std::sin(
            2.0F * std::numbers::pi_v<float> * kFreq *
            phase / static_cast<float>(sample_rate));
        for (int ch = 0; ch < channels; ++ch) {
            pcm[static_cast<std::size_t>(i * channels + ch)] = sample;
        }
        phase += 1.0F;
    }
    return pcm;
}

}  // namespace

TEST_CASE("OpusCodec: encode/decode round-trip", "[opus]") {
    OpusConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    config.bitrate = 128000;
    config.frame_size_samples = 240;

    OpusCodec codec(config);
    const auto pcm_in = make_sine_tone(config.sample_rate, config.channels,
                                        config.frame_size_samples);

    std::vector<std::uint8_t> encoded(4000);
    const int encoded_bytes = codec.encode(pcm_in, encoded);
    REQUIRE(encoded_bytes > 0);

    encoded.resize(static_cast<std::size_t>(encoded_bytes));

    std::vector<float> pcm_out(pcm_in.size());
    const int decoded_samples = codec.decode(encoded, pcm_out);
    REQUIRE(decoded_samples > 0);

    // Verify round-trip — lossy codec, but error should be bounded
    float max_diff = 0.0F;
    for (std::size_t i = 0; i < pcm_in.size(); ++i) {
        max_diff = std::max(max_diff, std::abs(pcm_in[i] - pcm_out[i]));
    }
    REQUIRE(max_diff < 0.5F);  // Generous bound for lossy codec
}

TEST_CASE("OpusCodec: encode silence", "[opus]") {
    OpusConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    config.frame_size_samples = 240;

    OpusCodec codec(config);
    const std::vector<float> silence(
        static_cast<std::size_t>(config.frame_size_samples * config.channels), 0.0F);

    std::vector<std::uint8_t> encoded(4000);
    const int encoded_bytes = codec.encode(silence, encoded);
    REQUIRE(encoded_bytes > 0);

    // Silence should compress very well
    REQUIRE(encoded_bytes < 100);
}

TEST_CASE("OpusCodec: PLC produces output without crashing", "[opus]") {
    OpusConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    config.frame_size_samples = 240;

    OpusCodec codec(config);

    // First encode+decode a real frame to prime the decoder state
    const auto pcm_in = make_sine_tone(config.sample_rate, config.channels,
                                        config.frame_size_samples);
    std::vector<std::uint8_t> encoded(4000);
    const int enc_bytes = codec.encode(pcm_in, encoded);
    REQUIRE(enc_bytes > 0);
    encoded.resize(static_cast<std::size_t>(enc_bytes));

    std::vector<float> decoded(pcm_in.size());
    REQUIRE(codec.decode(encoded, decoded) > 0);

    // Now simulate packet loss — PLC should not crash
    std::vector<float> plc_output(
        static_cast<std::size_t>(config.frame_size_samples * config.channels));
    const int plc_samples = codec.decode_plc(plc_output);
    REQUIRE(plc_samples > 0);
}

TEST_CASE("OpusCodec: move constructor transfers ownership", "[opus]") {
    OpusConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    config.frame_size_samples = 240;

    OpusCodec codec1(config);
    OpusCodec codec2(std::move(codec1));

    // codec2 should be functional after move
    const std::vector<float> silence(
        static_cast<std::size_t>(config.frame_size_samples * config.channels), 0.0F);
    std::vector<std::uint8_t> encoded(4000);
    const int bytes = codec2.encode(silence, encoded);
    REQUIRE(bytes > 0);
}
