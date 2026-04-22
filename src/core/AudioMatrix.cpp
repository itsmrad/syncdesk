#include "core/AudioMatrix.h"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace am::core {

AudioMatrix::AudioMatrix() {
    active_.store(&tables_[0], std::memory_order_release);
    pending_index_ = 1;
    spdlog::info("AudioMatrix initialized (double-buffered routing)");
}

AudioMatrix::~AudioMatrix() = default;

RouteTable& AudioMatrix::pending() noexcept {
    return tables_[pending_index_];
}

void AudioMatrix::add_route(DeviceId source, DeviceId sink, float gain) {
    auto& table = pending();

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
    auto& table = pending();
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
    auto& table = pending();
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
    // Swap: make the pending table active, then copy its state to the old
    // active table so it becomes the new pending.
    const int new_active_index = pending_index_;
    const int new_pending_index = (pending_index_ == 0) ? 1 : 0;

    active_.store(&tables_[new_active_index], std::memory_order_release);
    pending_index_ = new_pending_index;

    // Copy the now-active table into the new pending table so future edits
    // start from the current state.
    tables_[new_pending_index] = tables_[new_active_index];

    spdlog::debug("Route table committed ({} routes active)",
                  tables_[new_active_index].routes.size());
}

const RouteTable* AudioMatrix::active_table() const noexcept {
    return active_.load(std::memory_order_acquire);
}

std::size_t AudioMatrix::route_count() const noexcept {
    const auto* table = active_table();
    return table ? table->routes.size() : 0;
}

}  // namespace am::core
