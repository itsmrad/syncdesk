#include "ipc/ControlServer.h"
#include <spdlog/spdlog.h>
#include <nng/nng.h>
#include <nng/protocol/reqrep0/rep.h>

namespace am::ipc {

struct ControlServer::Impl {
    nng_socket socket{};
    bool running{false};
    std::thread worker;
};

ControlServer::ControlServer(std::uint16_t port)
    : impl_(std::make_unique<Impl>()), port_(port) {
    spdlog::info("ControlServer created on port {}", port_);
}

ControlServer::~ControlServer() { stop(); }

void ControlServer::start() {
    if (impl_->running) {
        return;
    }

    int rv = nng_rep0_open(&impl_->socket);
    if (rv != 0) {
        spdlog::error("NNG rep0 open failed: {}", nng_strerror(rv));
        return;
    }
    const auto url = "tcp://0.0.0.0:" + std::to_string(port_);
    rv = nng_listen(impl_->socket, url.c_str(), nullptr, 0);
    if (rv != 0) {
        spdlog::error("NNG listen failed: {}", nng_strerror(rv));
        nng_close(impl_->socket);
        impl_->socket = {};
        return;
    }
    impl_->running = true;
    spdlog::info("ControlServer listening on {}", url);

    impl_->worker = std::thread([this]() {
        spdlog::debug("ControlServer worker thread started");
        while (impl_->running) {
            void* buf = nullptr;
            size_t sz = 0;
            int recv_rv = nng_recv(impl_->socket, &buf, &sz, NNG_FLAG_ALLOC);
            if (recv_rv != 0) {
                if (recv_rv == NNG_ECLOSED || recv_rv == NNG_ESTATE) {
                    break; // Socket closed
                }
                spdlog::warn("NNG recv error: {}", nng_strerror(recv_rv));
                continue;
            }

            // TODO: Phase 0 intentionally omits request handling. 
            // In the future, deserialize FlatBuffers and process requests here.
            nng_free(buf, sz);

            // Send dummy reply for now to avoid breaking the REP protocol
            const char* reply = "ok";
            nng_send(impl_->socket, (void*)reply, 3, 0);
        }
        spdlog::debug("ControlServer worker thread exited");
    });
}

void ControlServer::stop() {
    if (impl_->running) {
        impl_->running = false;
        nng_close(impl_->socket);
        impl_->socket = {};
        if (impl_->worker.joinable()) {
            impl_->worker.join();
        }
        spdlog::info("ControlServer stopped");
    }
}

bool ControlServer::is_running() const noexcept { return impl_->running; }

}  // namespace am::ipc
