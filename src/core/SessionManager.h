#pragma once

/// @file SessionManager.h
/// @brief Owns the lifecycle of all peer connections.
///
/// When mDNS discovers a new peer, SessionManager initiates a handshake,
/// exchanges capabilities, and sets up the RTP stream.

#include "core/AudioMatrix.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace am::core {

/// Describes a remote peer's capabilities.
struct PeerInfo {
    DeviceId id{0};
    std::string name;
    std::string address;
    std::uint16_t port{0};
    std::uint32_t sample_rate{48000};
    std::uint32_t channels{2};
    bool connected{false};
};

/// Manages peer sessions — discovery, handshake, lifecycle.
class SessionManager {
public:
    using PeerDiscoveredCallback = std::function<void(const PeerInfo&)>;
    using PeerLostCallback = std::function<void(DeviceId)>;

    SessionManager();
    ~SessionManager();

    // Non-copyable
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    /// Register a newly discovered peer.
    void add_peer(PeerInfo info);

    /// Remove a peer that has disappeared.
    void remove_peer(DeviceId id);

    /// Get info about a specific peer (returns by value for safety).
    [[nodiscard]] std::optional<PeerInfo> get_peer(DeviceId id) const;

    /// Get all known peers.
    [[nodiscard]] std::vector<PeerInfo> get_all_peers() const;

    /// Set callback for when a new peer is discovered.
    void on_peer_discovered(PeerDiscoveredCallback callback);

    /// Set callback for when a peer is lost.
    void on_peer_lost(PeerLostCallback callback);

private:
    mutable std::mutex mutex_;
    std::unordered_map<DeviceId, PeerInfo> peers_;
    PeerDiscoveredCallback on_discovered_;
    PeerLostCallback on_lost_;
};

}  // namespace am::core
