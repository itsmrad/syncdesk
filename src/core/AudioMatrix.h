#pragma once

/// @file AudioMatrix.h
/// @brief N×M audio routing engine — the heart of AudioMatrix.
///
/// Routes are stored as a flat table: {source_id, sink_id, gain, enabled}.
/// The active table is swapped atomically via double-buffering so the audio
/// thread never blocks.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <mutex>

namespace am::core {

/// Unique identifier for an audio endpoint (source or sink).
using DeviceId = std::uint32_t;

/// A single route in the matrix.
struct Route {
    DeviceId source_id{0};
    DeviceId sink_id{0};
    float    gain{1.0F};       ///< Linear gain [0.0, 1.0+]
    bool     enabled{false};
};

/// Immutable snapshot of the routing table.
/// Audio thread reads from the active snapshot; control thread builds a new
/// one and swaps it in atomically.
struct RouteTable {
    std::vector<Route> routes;
};

/// The N×M routing engine.
///
/// Thread safety:
///   - `active_table()` is safe to call from the real-time audio thread.
///   - All mutating methods (`add_route`, `remove_route`, `set_gain`, `commit`)
///     must be called from the control thread only.
class AudioMatrix {
public:
    AudioMatrix();
    ~AudioMatrix();

    // Non-copyable, non-movable
    AudioMatrix(const AudioMatrix&) = delete;
    AudioMatrix& operator=(const AudioMatrix&) = delete;

    /// Add a route to the pending table.
    void add_route(DeviceId source, DeviceId sink, float gain = 1.0F);

    /// Remove a route from the pending table.
    void remove_route(DeviceId source, DeviceId sink);

    /// Update gain for an existing route.
    void set_gain(DeviceId source, DeviceId sink, float gain);

    /// Atomically publish the pending table so the audio thread sees it.
    void commit();

    /// Returns the currently active route table (lock-free read).
    /// The reader keeps the snapshot alive.
    [[nodiscard]] std::shared_ptr<const RouteTable> active_table() const noexcept;

    /// Number of active routes.
    [[nodiscard]] std::size_t route_count() const noexcept;

private:
    RouteTable pending_table_;
    std::shared_ptr<const RouteTable> active_;
};

}  // namespace am::core
