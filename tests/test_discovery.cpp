#include <catch2/catch_test_macros.hpp>
#include "discovery/MdnsService.h"

#include <chrono>
#include <thread>

using namespace am::discovery;

TEST_CASE("MdnsService: can create and start/stop", "[discovery]") {
    MdnsService svc("test-node", 0);
    REQUIRE_FALSE(svc.is_running());

    svc.start();
    
    // Poll for up to 1 second to verify it stays running and doesn't crash
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(500)) {
        if (!svc.is_running()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    REQUIRE(svc.is_running());

    svc.stop();
    REQUIRE_FALSE(svc.is_running());
}

TEST_CASE("MdnsService: known_services starts empty", "[discovery]") {
    MdnsService svc("test-node", 0);
    auto services = svc.known_services();
    REQUIRE(services.empty());
}
