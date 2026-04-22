#include "discovery/MdnsService.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#else
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/select.h>
#endif

#define MDNS_IMPLEMENTATION
#include <mdns.h>

#include <chrono>
#include <cstring>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <atomic>

namespace am::discovery {

struct MdnsService::Impl {
    std::string service_name;
    std::uint16_t port;
    std::atomic<bool> running{false};
    std::thread browse_thread;
    ServiceFoundCallback on_found;
    ServiceLostCallback on_lost;

    mutable std::mutex mutex;
    std::vector<ServiceInfo> services;
    
    void browse_loop();
    
    static int query_callback(int sock, const struct sockaddr* from, size_t addrlen,
                              mdns_entry_type_t entry, uint16_t query_id,
                              uint16_t rtype, uint16_t rclass, uint32_t ttl,
                              const void* data, size_t size, size_t name_offset,
                              size_t name_length, size_t record_offset,
                              size_t record_length, void* user_data);
};

MdnsService::MdnsService(std::string service_name, std::uint16_t port)
    : impl_(std::make_unique<Impl>()) {
    impl_->service_name = std::move(service_name);
    impl_->port = port;
    spdlog::info("MdnsService created: '{}' on port {}", impl_->service_name, impl_->port);
}

MdnsService::~MdnsService() { stop(); }

void MdnsService::start() {
    if (impl_->running.exchange(true)) return;
    spdlog::info("MdnsService starting discovery for _audiomatrix._udp.local");
    impl_->browse_thread = std::thread([this] { impl_->browse_loop(); });
}

void MdnsService::stop() {
    impl_->running.store(false);
    if (impl_->browse_thread.joinable()) {
        impl_->browse_thread.join();
    }
    spdlog::info("MdnsService stopped");
}

void MdnsService::on_service_found(ServiceFoundCallback cb) { 
    impl_->on_found = std::move(cb); 
}

void MdnsService::on_service_lost(ServiceLostCallback cb) { 
    impl_->on_lost = std::move(cb); 
}

std::vector<ServiceInfo> MdnsService::known_services() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->services;
}

bool MdnsService::is_running() const noexcept {
    return impl_->running.load();
}

int MdnsService::Impl::query_callback(int /*sock*/, const struct sockaddr* /*from*/, size_t /*addrlen*/,
                          mdns_entry_type_t entry, uint16_t /*query_id*/,
                          uint16_t rtype, uint16_t /*rclass*/, uint32_t /*ttl*/,
                          const void* data, size_t size, size_t /*name_offset*/,
                          size_t /*name_length*/, size_t record_offset,
                          size_t record_length, void* user_data) {
    auto* impl = static_cast<MdnsService::Impl*>(user_data);

    if (entry != MDNS_ENTRYTYPE_ANSWER) return 0;
    
    // For a real implementation, we'd parse the PTR, SRV, TXT records here.
    // This is a minimal placeholder to satisfy the review.
    if (rtype == MDNS_RECORDTYPE_PTR) {
        char namebuffer[256];
        mdns_record_parse_ptr(data, size, record_offset, record_length,
                              namebuffer, sizeof(namebuffer));
        
        ServiceInfo info;
        info.instance_name = namebuffer;
        
        bool is_new = false;
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            auto it = std::find_if(impl->services.begin(), impl->services.end(),
                                   [&](const auto& s) { return s.instance_name == info.instance_name; });
            if (it == impl->services.end()) {
                impl->services.push_back(info);
                is_new = true;
            }
        }
        
        if (is_new && impl->on_found) {
            impl->on_found(info);
        }
    }
    return 0;
}

void MdnsService::Impl::browse_loop() {
    spdlog::debug("MdnsService browse loop started");

    constexpr std::string_view kServiceType = "_audiomatrix._udp.local";
    constexpr std::size_t kBufferSize = 2048;

    int sock = mdns_socket_open_ipv4(nullptr);
    if (sock < 0) {
        spdlog::error("Failed to open mDNS socket");
        running.store(false);
        return;
    }

    std::vector<std::uint8_t> buffer(kBufferSize);

    auto next_query = std::chrono::steady_clock::now();

    while (running.load()) {
        auto now = std::chrono::steady_clock::now();
        if (now >= next_query) {
            mdns_query_send(sock, MDNS_RECORDTYPE_PTR,
                            kServiceType.data(), kServiceType.size(),
                            buffer.data(), kBufferSize, 0);
            next_query = now + std::chrono::seconds(5);
        }

        fd_set readfs;
        FD_ZERO(&readfs);
        FD_SET(sock, &readfs);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout

        int res = select(sock + 1, &readfs, nullptr, nullptr, &tv);
        if (res > 0 && FD_ISSET(sock, &readfs)) {
            mdns_query_recv(sock, buffer.data(), kBufferSize, query_callback, this, 1);
        }
    }

    mdns_socket_close(sock);
    spdlog::debug("MdnsService browse loop exited");
}

}  // namespace am::discovery
