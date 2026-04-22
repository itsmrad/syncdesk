#include "platform/AudioEngine.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <spdlog/spdlog.h>

namespace am::platform {

struct AudioEngine::Impl {
    ma_context context{};
    ma_device device{};
    bool context_initialized{false};
    bool device_initialized{false};
    AudioCallback user_callback;
};

AudioEngine::AudioEngine()
    : impl_(std::make_unique<Impl>()) {
    ma_context_config ctx_config = ma_context_config_init();
    if (ma_context_init(nullptr, 0, &ctx_config, &impl_->context) != MA_SUCCESS) {
        spdlog::error("Failed to initialize miniaudio context");
        return;
    }
    impl_->context_initialized = true;
    spdlog::info("AudioEngine initialized (miniaudio backend)");
}

AudioEngine::~AudioEngine() {
    stop();
    if (impl_->context_initialized) {
        ma_context_uninit(&impl_->context);
    }
}

std::vector<AudioDeviceInfo> AudioEngine::enumerate_devices() const {
    std::vector<AudioDeviceInfo> result;
    if (!impl_->context_initialized) return result;

    ma_device_info* playback_infos = nullptr;
    ma_uint32 playback_count = 0;
    ma_device_info* capture_infos = nullptr;
    ma_uint32 capture_count = 0;

    if (ma_context_get_devices(&impl_->context,
            &playback_infos, &playback_count,
            &capture_infos, &capture_count) != MA_SUCCESS) {
        spdlog::error("Failed to enumerate audio devices");
        return result;
    }

    std::uint32_t id_counter = 1;
    for (ma_uint32 i = 0; i < playback_count; ++i) {
        AudioDeviceInfo info;
        info.name = playback_infos[i].name;
        info.id = id_counter++;
        info.is_playback = true;
        info.is_default = playback_infos[i].isDefault != 0;
        result.push_back(std::move(info));
    }
    for (ma_uint32 i = 0; i < capture_count; ++i) {
        AudioDeviceInfo info;
        info.name = capture_infos[i].name;
        info.id = id_counter++;
        info.is_capture = true;
        info.is_default = capture_infos[i].isDefault != 0;
        result.push_back(std::move(info));
    }

    spdlog::info("Enumerated {} playback + {} capture devices",
                 playback_count, capture_count);
    return result;
}

bool AudioEngine::start_capture(std::uint32_t /*device_id*/, AudioCallback callback) {
    impl_->user_callback = std::move(callback);
    // TODO: implement device selection and start capture
    spdlog::info("AudioEngine capture start (stub)");
    return true;
}

bool AudioEngine::start_playback(std::uint32_t /*device_id*/, AudioCallback callback) {
    impl_->user_callback = std::move(callback);
    // TODO: implement device selection and start playback
    spdlog::info("AudioEngine playback start (stub)");
    return true;
}

void AudioEngine::stop() {
    if (impl_->device_initialized) {
        ma_device_uninit(&impl_->device);
        impl_->device_initialized = false;
    }
}

bool AudioEngine::is_running() const noexcept {
    // Only check the flag — don't call ma_device_is_started on
    // an uninitialized device (UB).
    return impl_->device_initialized;
}

}  // namespace am::platform
