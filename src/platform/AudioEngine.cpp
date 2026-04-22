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
    StreamHandle active_handle{0};
    StreamHandle next_handle{1};

    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        auto* impl = static_cast<AudioEngine::Impl*>(pDevice->pUserData);
        if (impl && impl->user_callback) {
            impl->user_callback(static_cast<float*>(pOutput), static_cast<const float*>(pInput), frameCount);
        }
    }
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

AudioEngine::StreamHandle AudioEngine::start_capture(std::uint32_t /*device_id*/, AudioCallback callback) {
    if (impl_->device_initialized) {
        spdlog::error("AudioEngine is already running a stream");
        return kInvalidHandle;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 2;
    config.sampleRate = 48000;
    config.dataCallback = Impl::data_callback;
    config.pUserData = impl_.get();

    if (ma_device_init(&impl_->context, &config, &impl_->device) != MA_SUCCESS) {
        spdlog::error("Failed to initialize capture device");
        return kInvalidHandle;
    }

    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        spdlog::error("Failed to start capture device");
        ma_device_uninit(&impl_->device);
        return kInvalidHandle;
    }

    impl_->user_callback = std::move(callback);
    impl_->device_initialized = true;
    impl_->active_handle = impl_->next_handle++;
    
    spdlog::info("AudioEngine capture started (handle={})", impl_->active_handle);
    return impl_->active_handle;
}

AudioEngine::StreamHandle AudioEngine::start_playback(std::uint32_t /*device_id*/, AudioCallback callback) {
    if (impl_->device_initialized) {
        spdlog::error("AudioEngine is already running a stream");
        return kInvalidHandle;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 48000;
    config.dataCallback = Impl::data_callback;
    config.pUserData = impl_.get();

    if (ma_device_init(&impl_->context, &config, &impl_->device) != MA_SUCCESS) {
        spdlog::error("Failed to initialize playback device");
        return kInvalidHandle;
    }

    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        spdlog::error("Failed to start playback device");
        ma_device_uninit(&impl_->device);
        return kInvalidHandle;
    }

    impl_->user_callback = std::move(callback);
    impl_->device_initialized = true;
    impl_->active_handle = impl_->next_handle++;

    spdlog::info("AudioEngine playback started (handle={})", impl_->active_handle);
    return impl_->active_handle;
}

void AudioEngine::stop() {
    if (impl_->device_initialized) {
        ma_device_uninit(&impl_->device);
        impl_->device_initialized = false;
        impl_->active_handle = kInvalidHandle;
        spdlog::info("AudioEngine stopped all streams");
    }
}

void AudioEngine::stop(StreamHandle handle) {
    if (impl_->device_initialized && impl_->active_handle == handle) {
        stop();
    }
}

bool AudioEngine::is_running() const noexcept {
    return impl_->device_initialized;
}

bool AudioEngine::is_running(StreamHandle handle) const noexcept {
    return impl_->device_initialized && impl_->active_handle == handle;
}

}  // namespace am::platform
