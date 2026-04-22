#include <catch2/catch_test_macros.hpp>
#include "transport/RtpSender.h"

#include <array>
#include <cstdint>

using namespace am::transport;

TEST_CASE("RtpHeader: serialize and deserialize round-trip", "[rtp]") {
    RtpHeader original;
    original.version = 2;
    original.padding = false;
    original.extension = false;
    original.csrc_count = 0;
    original.marker = true;
    original.payload_type = 111;
    original.sequence_number = 12345;
    original.timestamp = 480000;
    original.ssrc = 0xDEADBEEF;

    const auto bytes = original.serialize();
    REQUIRE(bytes.size() == 12);

    const auto recovered = RtpHeader::deserialize(
        std::span<const std::uint8_t, 12>(bytes));

    REQUIRE(recovered.version == 2);
    REQUIRE(recovered.marker == true);
    REQUIRE(recovered.payload_type == 111);
    REQUIRE(recovered.sequence_number == 12345);
    REQUIRE(recovered.timestamp == 480000);
    REQUIRE(recovered.ssrc == 0xDEADBEEF);
}

TEST_CASE("RtpHeader: version field is correct", "[rtp]") {
    RtpHeader h;
    h.version = 2;
    const auto bytes = h.serialize();

    // Version should be in the top 2 bits of byte 0
    REQUIRE(((bytes[0] >> 6) & 0x3) == 2);
}

TEST_CASE("RtpHeader: sequence number wraps at 16-bit", "[rtp]") {
    RtpHeader h;
    h.sequence_number = 65535;
    
    // Simulate sender incrementing sequence
    h.sequence_number++; 
    
    const auto bytes = h.serialize();
    const auto recovered = RtpHeader::deserialize(
        std::span<const std::uint8_t, 12>(bytes));
    REQUIRE(recovered.sequence_number == 0);
}

TEST_CASE("RtpHeader: padding, extension, and csrc_count round-trip", "[rtp]") {
    RtpHeader original;
    original.padding = true;
    original.extension = true;
    original.csrc_count = 5;
    
    const auto bytes = original.serialize();
    const auto recovered = RtpHeader::deserialize(
        std::span<const std::uint8_t, 12>(bytes));
        
    REQUIRE(recovered.padding == true);
    REQUIRE(recovered.extension == true);
    REQUIRE(recovered.csrc_count == 5);
}
