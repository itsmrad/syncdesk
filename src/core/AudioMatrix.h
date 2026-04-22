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

    // Non-copyable, non-movable (owns atomic state)
    AudioMatrix(const AudioMatrix&) = delete;
    AudioMatrix& operator=(const AudioMatrix&) = delete;
    AudioMatrix(AudioMatrix&&) = delete;
    AudioMatrix& operator=(AudioMatrix&&) = delete;

    /// Add a route to the pending table.
    void add_route(DeviceId source, DeviceId sink, float gain = 1.0F);

    /// Remove a route from the pending table.
    void remove_route(DeviceId source, DeviceId sink);

    /// Update gain for an existing route.
    void set_gain(DeviceId source, DeviceId sink, float gain);

    /// Atomically publish the pending table so the audio thread sees it.
    void commit();

    /// Returns the currently active route table (lock-free read).
    [[nodiscard]] const RouteTable* active_table() const noexcept;

    /// Number of active routes.
    [[nodiscard]] std::size_t route_count() const noexcept;

private:
    RouteTable tables_[2];                    ///< Double-buffered tables.
    std::atomic<RouteTable*> active_{nullptr}; ///< Points to the live table.
    int pending_index_{0};                     ///< Index of the table being edited.

    [[nodiscard]] RouteTable& pending() noexcept;
};

}  // namespace am::core
