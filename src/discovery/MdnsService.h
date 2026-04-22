#pragma once

/// @file MdnsService.h
/// @brief Zero-config LAN discovery using mDNS/DNS-SD.

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace am::discovery {

struct ServiceInfo {
    std::string instance_name;
    std::string hostname;
    std::string address;
    std::uint16_t port{0};
    std::string txt_record;
};

using ServiceFoundCallback = std::function<void(const ServiceInfo&)>;
using ServiceLostCallback  = std::function<void(const std::string&)>;

class MdnsService {
public:
    explicit MdnsService(std::string service_name, std::uint16_t port);
    ~MdnsService();
    MdnsService(const MdnsService&) = delete;
    MdnsService& operator=(const MdnsService&) = delete;

    void start();
    void stop();
    void on_service_found(ServiceFoundCallback callback);
    void on_service_lost(ServiceLostCallback callback);
    [[nodiscard]] std::vector<ServiceInfo> known_services() const;
    [[nodiscard]] bool is_running() const noexcept { return running_.load(); }

private:
    std::string service_name_;
    std::uint16_t port_;
    std::atomic<bool> running_{false};
    std::thread browse_thread_;
    ServiceFoundCallback on_found_;
    ServiceLostCallback on_lost_;
    struct Impl;
    std::unique_ptr<Impl> impl_;
    void browse_loop();
};

}  // namespace am::discovery
