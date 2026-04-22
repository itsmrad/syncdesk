#include "core/SessionManager.h"

#include <spdlog/spdlog.h>

namespace am::core {

SessionManager::SessionManager() {
    spdlog::info("SessionManager initialized");
}

SessionManager::~SessionManager() = default;

void SessionManager::add_peer(PeerInfo info) {
    const auto id = info.id;
    const auto name = info.name;

    PeerDiscoveredCallback callback_copy;
    PeerInfo peer_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        peers_[id] = std::move(info);
        peer_copy = peers_[id];
        callback_copy = on_discovered_;
    }
    // CP.22: Invoke callback outside the lock to avoid deadlock.
    spdlog::info("Peer added: {} (id={})", name, id);
    if (callback_copy) {
        callback_copy(peer_copy);
    }
}

void SessionManager::remove_peer(DeviceId id) {
    PeerLostCallback callback_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (const auto it = peers_.find(id); it != peers_.end()) {
            spdlog::info("Peer removed: {} (id={})", it->second.name, id);
            peers_.erase(it);
            callback_copy = on_lost_;
        } else {
            return;
        }
    }
    // CP.22: Invoke callback outside the lock to avoid deadlock.
    if (callback_copy) {
        callback_copy(id);
    }
}

std::optional<PeerInfo> SessionManager::get_peer(DeviceId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (const auto it = peers_.find(id); it != peers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<PeerInfo> SessionManager::get_all_peers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PeerInfo> result;
    result.reserve(peers_.size());
    for (const auto& [id, info] : peers_) {
        result.push_back(info);
    }
    return result;
}

void SessionManager::on_peer_discovered(PeerDiscoveredCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_discovered_ = std::move(callback);
}

void SessionManager::on_peer_lost(PeerLostCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_lost_ = std::move(callback);
}

}  // namespace am::core
