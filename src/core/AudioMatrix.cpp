#include "core/AudioMatrix.h"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace am::core {

AudioMatrix::AudioMatrix() {
    active_ = std::make_shared<RouteTable>();
    spdlog::info("AudioMatrix initialized (atomic shared_ptr routing)");
}

AudioMatrix::~AudioMatrix() = default;

void AudioMatrix::add_route(DeviceId source, DeviceId sink, float gain) {
    auto& table = pending_table_;

    // Check for duplicate
    const auto it = std::find_if(table.routes.begin(), table.routes.end(),
        [source, sink](const Route& r) {
            return r.source_id == source && r.sink_id == sink;
        });

    if (it != table.routes.end()) {
        it->gain = gain;
        it->enabled = true;
        spdlog::debug("Route {}->{} updated (gain={:.2f})", source, sink, gain);
    } else {
        table.routes.push_back({source, sink, gain, true});
        spdlog::info("Route {}->{} added (gain={:.2f})", source, sink, gain);
    }
}

void AudioMatrix::remove_route(DeviceId source, DeviceId sink) {
    auto& table = pending_table_;
    const auto it = std::remove_if(table.routes.begin(), table.routes.end(),
        [source, sink](const Route& r) {
            return r.source_id == source && r.sink_id == sink;
        });

    if (it != table.routes.end()) {
        table.routes.erase(it, table.routes.end());
        spdlog::info("Route {}->{} removed", source, sink);
    }
}

void AudioMatrix::set_gain(DeviceId source, DeviceId sink, float gain) {
    auto& table = pending_table_;
    const auto it = std::find_if(table.routes.begin(), table.routes.end(),
        [source, sink](const Route& r) {
            return r.source_id == source && r.sink_id == sink;
        });

    if (it != table.routes.end()) {
        it->gain = gain;
        spdlog::debug("Route {}->{} gain set to {:.2f}", source, sink, gain);
    }
}

void AudioMatrix::commit() {
    // Create a new snapshot from the pending table
    auto new_snapshot = std::make_shared<RouteTable>(pending_table_);
    
    // Atomically replace the active snapshot
    std::atomic_store_explicit(&active_, std::shared_ptr<const RouteTable>(new_snapshot), std::memory_order_release);

    spdlog::debug("Route table committed ({} routes active)", new_snapshot->routes.size());
}

std::shared_ptr<const RouteTable> AudioMatrix::active_table() const noexcept {
    return std::atomic_load_explicit(&active_, std::memory_order_acquire);
}

std::size_t AudioMatrix::route_count() const noexcept {
    auto table = active_table();
    return table ? table->routes.size() : 0;
}

}  // namespace am::core
