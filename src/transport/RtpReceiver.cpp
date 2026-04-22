#include "transport/RtpReceiver.h"

#include <array>
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <thread>

namespace am::transport {

struct RtpReceiver::Impl {
    asio::io_context io_context;
    asio::ip::udp::socket socket;
    asio::ip::udp::endpoint sender_endpoint;
    std::array<std::uint8_t, 1500> recv_buffer{};  // MTU-sized buffer
    std::thread io_thread;

    explicit Impl(std::uint16_t port)
        : socket(io_context,
                 asio::ip::udp::endpoint(asio::ip::udp::v4(), port)) {}
};

RtpReceiver::RtpReceiver(std::uint16_t bind_port)
    : impl_(std::make_unique<Impl>(bind_port)) {
    spdlog::info("RtpReceiver listening on port {}", bind_port);
}

RtpReceiver::~RtpReceiver() {
    stop();
}

void RtpReceiver::on_packet(RtpPacketCallback callback) {
    callback_ = std::move(callback);
}

void RtpReceiver::start() {
    do_receive();
    impl_->io_thread = std::thread([this] {
        spdlog::debug("RtpReceiver I/O thread started");
        impl_->io_context.run();
        spdlog::debug("RtpReceiver I/O thread exited");
    });
}

void RtpReceiver::stop() {
    impl_->io_context.stop();
    if (impl_->io_thread.joinable()) {
        impl_->io_thread.join();
    }
}

void RtpReceiver::do_receive() {
    impl_->socket.async_receive_from(
        asio::buffer(impl_->recv_buffer),
        impl_->sender_endpoint,
        [this](const asio::error_code& ec, std::size_t bytes_received) {
            if (!ec && bytes_received > 12 && callback_) {
                // Parse RTP header (first 12 bytes)
                std::span<const std::uint8_t, 12> header_data(
                    impl_->recv_buffer.data(), 12);
                const auto header = RtpHeader::deserialize(header_data);

                // Payload is everything after the header
                std::span<const std::uint8_t> payload(
                    impl_->recv_buffer.data() + 12,
                    bytes_received - 12);

                callback_(header, payload);
                packets_received_.fetch_add(1, std::memory_order_relaxed);
            } else if (ec && ec != asio::error::operation_aborted) {
                spdlog::warn("RTP receive error: {}", ec.message());
            }

            // Continue receiving
            if (!impl_->io_context.stopped()) {
                do_receive();
            }
        });
}

}  // namespace am::transport
