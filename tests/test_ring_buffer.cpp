#include <catch2/catch_test_macros.hpp>
#include "core/RingBuffer.h"

using namespace am::core;

TEST_CASE("RingBuffer: push and pop round-trip", "[ringbuf]") {
    RingBuffer<int> rb(16);
    REQUIRE(rb.try_push(42));
    const auto val = rb.try_pop();
    REQUIRE(val.has_value());
    REQUIRE(val.value() == 42);
}

TEST_CASE("RingBuffer: pop from empty returns nullopt", "[ringbuf]") {
    RingBuffer<int> rb(16);
    REQUIRE_FALSE(rb.try_pop().has_value());
}

TEST_CASE("RingBuffer: push to full returns false", "[ringbuf]") {
    // ReaderWriterQueue uses the capacity as a hint (rounds up internally).
    // With capacity 1, the queue can hold exactly 1 element.
    RingBuffer<int> rb(1);
    REQUIRE(rb.try_push(1));
    REQUIRE_FALSE(rb.try_push(2));
}

TEST_CASE("RingBuffer: size_approx tracks elements", "[ringbuf]") {
    RingBuffer<int> rb(16);
    REQUIRE(rb.size_approx() == 0);
    (void)rb.try_push(1);
    (void)rb.try_push(2);
    REQUIRE(rb.size_approx() == 2);
    (void)rb.try_pop();
    REQUIRE(rb.size_approx() == 1);
}

TEST_CASE("RingBuffer: FIFO ordering preserved", "[ringbuf]") {
    RingBuffer<int> rb(16);
    for (int i = 0; i < 5; ++i) {
        REQUIRE(rb.try_push(i));
    }
    for (int i = 0; i < 5; ++i) {
        const auto val = rb.try_pop();
        REQUIRE(val.has_value());
        REQUIRE(val.value() == i);
    }
}

TEST_CASE("RingBuffer: move semantics", "[ringbuf]") {
    RingBuffer<std::string> rb(8);
    std::string s = "hello";
    REQUIRE(rb.try_push(std::move(s)));
    const auto val = rb.try_pop();
    REQUIRE(val.has_value());
    REQUIRE(val.value() == "hello");
}
