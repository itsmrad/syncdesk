#pragma once

/// @file AudioEngine.h
/// @brief Platform audio abstraction via miniaudio.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace am::platform {

struct AudioDeviceInfo {
    std::string name;
    std::uint32_t id{0};
    bool is_capture{false};
    bool is_playback{false};
    std::uint32_t sample_rate{48000};
    std::uint32_t channels{2};
    bool is_default{false};
};

using AudioCallback = std::function<void(float* output, const float* input,
                                          std::uint32_t frame_count)>;

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    [[nodiscard]] std::vector<AudioDeviceInfo> enumerate_devices() const;
    bool start_capture(std::uint32_t device_id, AudioCallback callback);
    bool start_playback(std::uint32_t device_id, AudioCallback callback);
    void stop();
    [[nodiscard]] bool is_running() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace am::platform
