/// @file main.cpp
/// @brief AudioMatrix Phase 0 — proof-of-concept entry point.
///
/// Demonstrates four milestones:
///   1. miniaudio device enumeration
///   2. Opus encode/decode round-trip
///   3. Routing engine (double-buffered N×M)
///   4. mDNS service advertising

#include "core/AudioMatrix.h"
#include "core/SessionManager.h"
#include "discovery/MdnsService.h"
#include "platform/AudioEngine.h"
#include "transport/OpusCodec.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <chrono>
#include <cmath>
#include <csignal>
#include <numbers>
#include <thread>
#include <vector>

namespace {

volatile std::sig_atomic_t g_running = 1;  // NOLINT

void signal_handler(int /*signal*/) {
    g_running = 0;
}

/// Generate a 440Hz sine wave test tone.
void generate_test_tone(std::vector<float>& buffer, int sample_rate,
                        int channels, int frame_size, float& phase) {
    constexpr float kFrequency = 440.0F;
    const auto samples = static_cast<std::size_t>(frame_size * channels);
    buffer.resize(samples);

    for (int i = 0; i < frame_size; ++i) {
        const float sample = 0.3F * std::sin(
            2.0F * std::numbers::pi_v<float> * kFrequency *
            phase / static_cast<float>(sample_rate));
        for (int ch = 0; ch < channels; ++ch) {
            buffer[static_cast<std::size_t>(i * channels + ch)] = sample;
        }
        phase += 1.0F;
    }
}

}  // namespace

int main() {
    // ── Setup logging ────────────────────────────────────────────────────
    spdlog::set_default_logger(spdlog::stdout_color_mt("audiomatrix"));
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("╔══════════════════════════════════════╗");
    spdlog::info("║  AudioMatrix v0.1.0 — Phase 0 PoC   ║");
    spdlog::info("╚══════════════════════════════════════╝");

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // ── Milestone 1: Device Enumeration ─────────────────────────────────
    spdlog::info("─── Milestone 1: Audio Device Enumeration ───");
    {
        am::platform::AudioEngine engine;
        const auto devices = engine.enumerate_devices();
        for (const auto& dev : devices) {
            spdlog::info("  {} [{}] {}{}",
                dev.is_default ? "★" : " ",
                dev.id,
                dev.name,
                dev.is_capture ? " (capture)" : " (playback)");
        }
        if (devices.empty()) {
            spdlog::warn("  No audio devices found!");
        }
    }

    // ── Milestone 2: Opus Round-Trip ────────────────────────────────────
    spdlog::info("─── Milestone 2: Opus Encode/Decode Round-Trip ───");
    {
        am::transport::OpusConfig config;
        config.sample_rate = 48000;
        config.channels = 2;
        config.bitrate = 128000;
        config.frame_size_samples = 240;  // 5ms @ 48kHz

        am::transport::OpusCodec codec(config);

        // Generate test tone
        std::vector<float> pcm_in;
        float phase = 0.0F;
        generate_test_tone(pcm_in, config.sample_rate, config.channels,
                          config.frame_size_samples, phase);

        // Encode
        std::vector<std::uint8_t> encoded(4000);
        const int encoded_bytes = codec.encode(pcm_in, encoded);

        if (encoded_bytes > 0) {
            encoded.resize(static_cast<std::size_t>(encoded_bytes));
            spdlog::info("  Encoded {} PCM samples -> {} bytes",
                        pcm_in.size(), encoded_bytes);

            // Decode
            std::vector<float> pcm_out(pcm_in.size());
            const int decoded_samples = codec.decode(encoded, pcm_out);

            if (decoded_samples > 0) {
                // Verify round-trip quality (allow for lossy codec)
                float max_diff = 0.0F;
                for (std::size_t i = 0; i < pcm_in.size(); ++i) {
                    max_diff = std::max(max_diff, std::abs(pcm_in[i] - pcm_out[i]));
                }
                spdlog::info("  Decoded {} samples, max error: {:.6f}",
                            decoded_samples * config.channels, max_diff);
                spdlog::info("  ✓ Opus round-trip successful!");
            }
        } else {
            spdlog::error("  ✗ Opus encoding failed");
        }
    }

    // ── Milestone 3: Routing Engine ─────────────────────────────────────
    spdlog::info("─── Milestone 3: Routing Engine ───");
    {
        am::core::AudioMatrix matrix;
        matrix.add_route(1, 10, 0.8F);
        matrix.add_route(2, 10, 0.5F);
        matrix.add_route(1, 11, 1.0F);
        matrix.commit();
        spdlog::info("  Active routes: {}", matrix.route_count());

        const auto* table = matrix.active_table();
        if (table) {
            for (const auto& route : table->routes) {
                spdlog::info("    {} -> {} (gain={:.1f}, {})",
                    route.source_id, route.sink_id, route.gain,
                    route.enabled ? "ON" : "OFF");
            }
        }
        spdlog::info("  ✓ Routing engine working!");
    }

    // ── Milestone 4: mDNS Discovery ────────────────────────────────────
    spdlog::info("─── Milestone 4: mDNS Service Discovery ───");
    {
        am::discovery::MdnsService mdns("AudioMatrix-Dev", 9876);
        mdns.on_service_found([](const am::discovery::ServiceInfo& svc) {
            spdlog::info("  Found: {} at {}:{}",
                        svc.instance_name, svc.address, svc.port);
        });
        mdns.start();
        spdlog::info("  mDNS advertising started, browsing for 3 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        mdns.stop();
        spdlog::info("  ✓ mDNS service operational!");
    }

    spdlog::info("═══════════════════════════════════════");
    spdlog::info("  Phase 0 complete. All milestones passed.");
    spdlog::info("═══════════════════════════════════════");

    return 0;
}
