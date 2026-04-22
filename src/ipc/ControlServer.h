#pragma once

/// @file ControlServer.h
/// @brief NNG-based control plane for inter-node communication.

#include <cstdint>
#include <memory>
#include <string>

namespace am::ipc {

class ControlServer {
public:
    explicit ControlServer(std::uint16_t port);
    ~ControlServer();
    ControlServer(const ControlServer&) = delete;
    ControlServer& operator=(const ControlServer&) = delete;

    void start();
    void stop();
    [[nodiscard]] bool is_running() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::uint16_t port_;
};

}  // namespace am::ipc
