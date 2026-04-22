#include <catch2/catch_test_macros.hpp>
#include "core/SessionManager.h"

#include <string>
#include <vector>

using namespace am::core;

namespace {

PeerInfo make_peer(DeviceId id, const std::string& name) {
    PeerInfo p;
    p.id = id;
    p.name = name;
    p.address = "192.168.1." + std::to_string(id);
    p.port = static_cast<std::uint16_t>(9000 + id);
    return p;
}

}  // namespace

TEST_CASE("SessionManager: add and get peer", "[session]") {
    SessionManager mgr;
    mgr.add_peer(make_peer(1, "node-a"));

    const auto peer = mgr.get_peer(1);
    REQUIRE(peer.has_value());
    REQUIRE(peer->name == "node-a");
    REQUIRE(peer->id == 1);
}

TEST_CASE("SessionManager: get nonexistent peer returns nullopt", "[session]") {
    SessionManager mgr;
    REQUIRE_FALSE(mgr.get_peer(99).has_value());
}

TEST_CASE("SessionManager: remove peer", "[session]") {
    SessionManager mgr;
    mgr.add_peer(make_peer(1, "node-a"));
    mgr.add_peer(make_peer(2, "node-b"));

    mgr.remove_peer(1);
    REQUIRE_FALSE(mgr.get_peer(1).has_value());
    REQUIRE(mgr.get_peer(2).has_value());
}

TEST_CASE("SessionManager: remove nonexistent peer is no-op", "[session]") {
    SessionManager mgr;
    REQUIRE_NOTHROW(mgr.remove_peer(99));
}

TEST_CASE("SessionManager: get_all_peers", "[session]") {
    SessionManager mgr;
    mgr.add_peer(make_peer(1, "node-a"));
    mgr.add_peer(make_peer(2, "node-b"));

    const auto peers = mgr.get_all_peers();
    REQUIRE(peers.size() == 2);
}

TEST_CASE("SessionManager: discovered callback fires", "[session]") {
    SessionManager mgr;
    bool callback_fired = false;
    DeviceId received_id = 0;

    mgr.on_peer_discovered([&](const PeerInfo& p) {
        callback_fired = true;
        received_id = p.id;
    });

    mgr.add_peer(make_peer(42, "test-node"));
    REQUIRE(callback_fired);
    REQUIRE(received_id == 42);
}

TEST_CASE("SessionManager: lost callback fires on remove", "[session]") {
    SessionManager mgr;
    bool callback_fired = false;
    DeviceId lost_id = 0;

    mgr.on_peer_lost([&](DeviceId id) {
        callback_fired = true;
        lost_id = id;
    });

    mgr.add_peer(make_peer(7, "ephemeral"));
    mgr.remove_peer(7);
    REQUIRE(callback_fired);
    REQUIRE(lost_id == 7);
}

TEST_CASE("SessionManager: callback does not fire for unknown remove", "[session]") {
    SessionManager mgr;
    bool callback_fired = false;
    mgr.on_peer_lost([&](DeviceId) { callback_fired = true; });

    mgr.remove_peer(999);
    REQUIRE_FALSE(callback_fired);
}
