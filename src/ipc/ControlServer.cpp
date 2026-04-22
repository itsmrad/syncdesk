#include "ipc/ControlServer.h"
#include <spdlog/spdlog.h>
#include <nng/nng.h>
#include <nng/protocol/reqrep0/rep.h>

namespace am::ipc {

struct ControlServer::Impl {
    nng_socket socket{};
    bool running{false};
};

ControlServer::ControlServer(std::uint16_t port)
    : impl_(std::make_unique<Impl>()), port_(port) {
    spdlog::info("ControlServer created on port {}", port_);
}

ControlServer::~ControlServer() { stop(); }

void ControlServer::start() {
    int rv = nng_rep0_open(&impl_->socket);
    if (rv != 0) {
        spdlog::error("NNG rep0 open failed: {}", nng_strerror(rv));
        return;
    }
    const auto url = "tcp://0.0.0.0:" + std::to_string(port_);
    rv = nng_listen(impl_->socket, url.c_str(), nullptr, 0);
    if (rv != 0) {
        spdlog::error("NNG listen failed: {}", nng_strerror(rv));
        return;
    }
    impl_->running = true;
    spdlog::info("ControlServer listening on {}", url);
}

void ControlServer::stop() {
    if (impl_->running) {
        nng_close(impl_->socket);
        impl_->running = false;
        spdlog::info("ControlServer stopped");
    }
}

bool ControlServer::is_running() const noexcept { return impl_->running; }

}  // namespace am::ipc
