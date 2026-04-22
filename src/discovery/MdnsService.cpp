#include "discovery/MdnsService.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#else
#include <netdb.h>
#include <ifaddrs.h>
#endif

#define MDNS_IMPLEMENTATION
#include <mdns.h>

#include <chrono>
#include <cstring>
#include <mutex>
#include <vector>

namespace am::discovery {

struct MdnsService::Impl {
    mutable std::mutex mutex;
    std::vector<ServiceInfo> services;
    int socket_fd{-1};
};

MdnsService::MdnsService(std::string service_name, std::uint16_t port)
    : service_name_(std::move(service_name))
    , port_(port)
    , impl_(std::make_unique<Impl>()) {
    spdlog::info("MdnsService created: '{}' on port {}", service_name_, port_);
}

MdnsService::~MdnsService() { stop(); }

void MdnsService::start() {
    if (running_.exchange(true)) return;
    spdlog::info("MdnsService starting discovery for _audiomatrix._udp.local");
    browse_thread_ = std::thread([this] { browse_loop(); });
}

void MdnsService::stop() {
    running_.store(false);
    if (browse_thread_.joinable()) browse_thread_.join();
    spdlog::info("MdnsService stopped");
}

void MdnsService::on_service_found(ServiceFoundCallback cb) { on_found_ = std::move(cb); }
void MdnsService::on_service_lost(ServiceLostCallback cb) { on_lost_ = std::move(cb); }

std::vector<ServiceInfo> MdnsService::known_services() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->services;
}

void MdnsService::browse_loop() {
    spdlog::debug("MdnsService browse loop started");

    constexpr std::string_view kServiceType = "_audiomatrix._udp.local";
    constexpr std::size_t kBufferSize = 2048;

    while (running_.load()) {
        // Open a socket for mDNS queries
        int sock = mdns_socket_open_ipv4(nullptr);
        if (sock < 0) {
            spdlog::warn("Failed to open mDNS socket, retrying...");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        // R.10: No malloc/free in C++ — use stack-allocated buffer.
        std::vector<std::uint8_t> buffer(kBufferSize);
        mdns_query_send(sock, MDNS_RECORDTYPE_PTR,
                       kServiceType.data(), kServiceType.size(),
                       buffer.data(), kBufferSize, 0);

        mdns_socket_close(sock);
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    spdlog::debug("MdnsService browse loop exited");
}

}  // namespace am::discovery
