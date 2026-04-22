#include <catch2/catch_test_macros.hpp>
#include "discovery/MdnsService.h"

#include <chrono>
#include <thread>

using namespace am::discovery;

TEST_CASE("MdnsService: can create and start/stop", "[discovery]") {
    MdnsService svc("test-node", 9876);
    REQUIRE_FALSE(svc.is_running());

    svc.start();
    REQUIRE(svc.is_running());

    // Let it browse briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    svc.stop();
    REQUIRE_FALSE(svc.is_running());
}

TEST_CASE("MdnsService: known_services starts empty", "[discovery]") {
    MdnsService svc("test-node", 9876);
    auto services = svc.known_services();
    REQUIRE(services.empty());
}
