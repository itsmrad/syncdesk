#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/AudioMatrix.h"

using namespace am::core;

TEST_CASE("AudioMatrix: add and commit routes", "[matrix]") {
    AudioMatrix matrix;
    REQUIRE(matrix.route_count() == 0);

    matrix.add_route(1, 10, 0.5F);
    matrix.add_route(2, 11, 1.0F);
    matrix.commit();

    REQUIRE(matrix.route_count() == 2);
}

TEST_CASE("AudioMatrix: remove route", "[matrix]") {
    AudioMatrix matrix;
    matrix.add_route(1, 10);
    matrix.add_route(2, 11);
    matrix.commit();
    REQUIRE(matrix.route_count() == 2);

    matrix.remove_route(1, 10);
    matrix.commit();
    REQUIRE(matrix.route_count() == 1);
}

TEST_CASE("AudioMatrix: set gain", "[matrix]") {
    AudioMatrix matrix;
    matrix.add_route(1, 10, 0.5F);
    matrix.commit();

    matrix.set_gain(1, 10, 0.8F);
    matrix.commit();

    const auto* table = matrix.active_table();
    REQUIRE(table != nullptr);
    REQUIRE(table->routes.size() == 1);
    REQUIRE(table->routes[0].gain == Catch::Approx(0.8F));
}

TEST_CASE("AudioMatrix: duplicate add updates gain", "[matrix]") {
    AudioMatrix matrix;
    matrix.add_route(1, 10, 0.5F);
    matrix.add_route(1, 10, 0.9F);
    matrix.commit();

    REQUIRE(matrix.route_count() == 1);
    const auto* table = matrix.active_table();
    REQUIRE(table->routes[0].gain == Catch::Approx(0.9F));
}

TEST_CASE("AudioMatrix: active_table is lock-free readable", "[matrix]") {
    AudioMatrix matrix;
    matrix.add_route(1, 10);
    matrix.commit();

    // Simulate reading from "audio thread"
    const auto* table = matrix.active_table();
    REQUIRE(table != nullptr);
    REQUIRE(table->routes.size() == 1);
    REQUIRE(table->routes[0].source_id == 1);
    REQUIRE(table->routes[0].sink_id == 10);
}

TEST_CASE("AudioMatrix: empty commit is safe", "[matrix]") {
    AudioMatrix matrix;
    REQUIRE_NOTHROW(matrix.commit());
    REQUIRE(matrix.route_count() == 0);
}

TEST_CASE("AudioMatrix: remove nonexistent route is no-op", "[matrix]") {
    AudioMatrix matrix;
    matrix.add_route(1, 10);
    matrix.commit();

    matrix.remove_route(99, 99);
    matrix.commit();
    REQUIRE(matrix.route_count() == 1);
}

TEST_CASE("AudioMatrix: set_gain on nonexistent route is no-op", "[matrix]") {
    AudioMatrix matrix;
    matrix.add_route(1, 10, 0.5F);
    matrix.commit();

    REQUIRE_NOTHROW(matrix.set_gain(99, 99, 0.9F));
    matrix.commit();
    REQUIRE(matrix.active_table()->routes[0].gain == Catch::Approx(0.5F));
}

TEST_CASE("AudioMatrix: multiple commits preserve state", "[matrix]") {
    AudioMatrix matrix;
    matrix.add_route(1, 10, 0.5F);
    matrix.commit();
    matrix.commit();  // Double commit should be safe
    matrix.commit();

    REQUIRE(matrix.route_count() == 1);
    REQUIRE(matrix.active_table()->routes[0].gain == Catch::Approx(0.5F));
}
