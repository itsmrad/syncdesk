#include "transport/OpusCodec.h"

#include <opus/opus.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>

namespace am::transport {

OpusCodec::OpusCodec(OpusConfig config)
    : config_(std::move(config)) {
    int err = 0;

    encoder_ = opus_encoder_create(
        config_.sample_rate, config_.channels,
        OPUS_APPLICATION_RESTRICTED_LOWDELAY, &err);
    if (err != OPUS_OK || encoder_ == nullptr) {
        throw std::runtime_error(
            std::string("Opus encoder creation failed: ") + opus_strerror(err));
    }

    // Configure for ultra-low latency
    opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(config_.bitrate));
    opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(5));  // Balance quality/CPU
    opus_encoder_ctl(encoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    opus_encoder_ctl(encoder_, OPUS_SET_INBAND_FEC(1));  // Built-in FEC

    decoder_ = opus_decoder_create(config_.sample_rate, config_.channels, &err);
    if (err != OPUS_OK || decoder_ == nullptr) {
        opus_encoder_destroy(encoder_);
        encoder_ = nullptr;
        throw std::runtime_error(
            std::string("Opus decoder creation failed: ") + opus_strerror(err));
    }

    spdlog::info("OpusCodec initialized (rate={}Hz, ch={}, bitrate={}kbps, frame={}samples)",
                 config_.sample_rate, config_.channels,
                 config_.bitrate / 1000, config_.frame_size_samples);
}

OpusCodec::~OpusCodec() {
    if (encoder_) { opus_encoder_destroy(encoder_); }
    if (decoder_) { opus_decoder_destroy(decoder_); }
}

OpusCodec::OpusCodec(OpusCodec&& other) noexcept
    : config_(std::move(other.config_))
    , encoder_(std::exchange(other.encoder_, nullptr))
    , decoder_(std::exchange(other.decoder_, nullptr)) {}

OpusCodec& OpusCodec::operator=(OpusCodec&& other) noexcept {
    if (this != &other) {
        if (encoder_) { opus_encoder_destroy(encoder_); }
        if (decoder_) { opus_decoder_destroy(decoder_); }
        config_ = std::move(other.config_);
        encoder_ = std::exchange(other.encoder_, nullptr);
        decoder_ = std::exchange(other.decoder_, nullptr);
    }
    return *this;
}

int OpusCodec::encode(std::span<const float> pcm,
                      std::span<std::uint8_t> output) {
    const int result = opus_encode_float(
        encoder_,
        pcm.data(),
        config_.frame_size_samples,
        output.data(),
        static_cast<opus_int32>(output.size()));

    if (result < 0) {
        spdlog::error("Opus encode error: {}", opus_strerror(result));
    }
    return result;
}

int OpusCodec::decode(std::span<const std::uint8_t> data,
                      std::span<float> output) {
    const int result = opus_decode_float(
        decoder_,
        data.data(),
        static_cast<opus_int32>(data.size()),
        output.data(),
        config_.frame_size_samples,
        0);  // No FEC for normal decode

    if (result < 0) {
        spdlog::error("Opus decode error: {}", opus_strerror(result));
    }
    return result;
}

int OpusCodec::decode_plc(std::span<float> output) {
    // Pass nullptr for data to trigger PLC
    const int result = opus_decode_float(
        decoder_,
        nullptr,
        0,
        output.data(),
        config_.frame_size_samples,
        0);

    if (result < 0) {
        spdlog::error("Opus PLC error: {}", opus_strerror(result));
    }
    return result;
}

}  // namespace am::transport
