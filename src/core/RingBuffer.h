#pragma once

/// @file RingBuffer.h
/// @brief Lock-free SPSC ring buffer — thin wrapper over cameron314/readerwriterqueue.
///
/// Used as the hand-off between the audio thread (producer) and the network
/// thread (consumer), and vice versa. The audio thread must never block.

#include <cstddef>
#include <optional>
#include <utility>
#include <readerwriterqueue.h>

namespace am::core {

/// Lock-free single-producer single-consumer ring buffer.
///
/// @tparam T  Element type (typically a fixed-size audio frame).
template <typename T>
class RingBuffer {
public:
    /// @param capacity  Maximum number of elements the buffer can hold.
    explicit RingBuffer(std::size_t capacity)
        : queue_(capacity) {}

    /// Try to enqueue an element (non-blocking, real-time safe).
    /// @return true if the element was enqueued, false if the buffer is full.
    [[nodiscard]] bool try_push(const T& item) noexcept {
        return queue_.try_enqueue(item);
    }

    /// Try to enqueue an element by move (non-blocking, real-time safe).
    [[nodiscard]] bool try_push(T&& item) noexcept {
        return queue_.try_enqueue(std::move(item));
    }

    /// Try to dequeue an element (non-blocking, real-time safe).
    /// @return The element if one was available, std::nullopt otherwise.
    [[nodiscard]] std::optional<T> try_pop() noexcept {
        T item;
        if (queue_.try_dequeue(item)) {
            return item;
        }
        return std::nullopt;
    }

    /// Approximate number of elements currently in the buffer.
    [[nodiscard]] std::size_t size_approx() const noexcept {
        return queue_.size_approx();
    }

private:
    moodycamel::ReaderWriterQueue<T> queue_;
};

}  // namespace am::core
