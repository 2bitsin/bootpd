#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <dhcp_packet_v4.hpp>

namespace
{
	const std::uint8_t test_mac[] = { 0x00, 0x1c, 0x7e, 0x35, 0xed, 0x20 };

	auto sample_packet() -> dhcp_packet_v4
	{
		dhcp_packet_v4 packet;
		packet.opcode(DHCP_OPCODE_REQUEST)
			.hardware_address(test_mac)
			.transaction_id(0xdeadbeef)
			.flags(DHCP_FLAGS_BROADCAST)
			.client_address(0x0a000005)
			.your_address(0x0a000002)
			.server_address(0x0a000001)
			.gateway_address(0x0a0000fe)
			.server_host_name("server")
			.boot_file_name("boot/pxelinux.0")
			.message_type(DHCP_MESSAGE_TYPE_DISCOVER);
		return packet;
	}
}

TEST(dhcp_packet, round_trips)
{
	const auto bits = serialize_to_vector(sample_packet());
	const dhcp_packet_v4 parsed(bits);

	EXPECT_EQ(parsed.opcode(), DHCP_OPCODE_REQUEST);
	EXPECT_EQ(parsed.hardware_type(), DHCP_HARDWARE_TYPE_ETHERNET);
	EXPECT_EQ(parsed.hardware_address_length(), 6u);
	EXPECT_EQ(std::vector<std::uint8_t>(parsed.hardware_address().begin(), parsed.hardware_address().end()),
		std::vector<std::uint8_t>(std::begin(test_mac), std::end(test_mac)));
	EXPECT_EQ(parsed.transaction_id(), 0xdeadbeefu);
	EXPECT_EQ(parsed.flags(), DHCP_FLAGS_BROADCAST);
	EXPECT_EQ(parsed.client_address(), 0x0a000005u);
	EXPECT_EQ(parsed.your_address(), 0x0a000002u);
	EXPECT_EQ(parsed.server_address(), 0x0a000001u);
	EXPECT_EQ(parsed.gateway_address(), 0x0a0000feu);
	EXPECT_EQ(parsed.server_host_name(), "server");
	EXPECT_EQ(parsed.boot_file_name(), "boot/pxelinux.0");
	EXPECT_TRUE(parsed.is_message_type(DHCP_MESSAGE_TYPE_DISCOVER));
}

TEST(dhcp_packet, has_the_rfc_field_layout)
{
	const auto bits = serialize_to_vector(sample_packet());
	EXPECT_EQ(bits[0], std::byte{ DHCP_OPCODE_REQUEST });
	EXPECT_EQ(bits[4], std::byte{ 0xde });       // xid
	EXPECT_EQ(bits[10], std::byte{ 0x80 });      // flags
	EXPECT_EQ(bits[28], std::byte{ 0x00 });      // chaddr
	EXPECT_EQ(bits[29], std::byte{ 0x1c });
	EXPECT_EQ(bits[44], std::byte{ 's' });       // sname
	EXPECT_EQ(bits[108], std::byte{ 'b' });      // file
	EXPECT_EQ(bits[236], std::byte{ 0x63 });     // magic cookie
}

TEST(dhcp_packet, is_padded_to_the_bootp_minimum)
{
	dhcp_packet_v4 packet;
	const auto bits = serialize_to_vector(packet);
	EXPECT_EQ(bits.size(), DHCP_MINIMUM_PACKET_SIZE);
}

TEST(dhcp_packet, clamps_an_oversized_hardware_address_length)
{
	// Regression : hlen straight from the wire was used as the chaddr length,
	// reading past the 16 byte field.
	auto bits = serialize_to_vector(sample_packet());
	bits[2] = std::byte{ 200 };
	const dhcp_packet_v4 parsed(bits);
	EXPECT_EQ(parsed.hardware_address().size(), 16u);
}

TEST(dhcp_packet, handles_unterminated_name_fields)
{
	auto bits = serialize_to_vector(sample_packet());
	for (std::size_t i = 44; i < 108 + 128; ++i)
		bits[i] = std::byte{ 'x' };
	const dhcp_packet_v4 parsed(bits);
	EXPECT_EQ(parsed.server_host_name().size(), 64u);
	EXPECT_EQ(parsed.boot_file_name().size(), 128u);
	std::ostringstream oss;
	EXPECT_NO_THROW(parsed.pretty_print(oss));
}

TEST(dhcp_packet, rejects_too_long_names)
{
	dhcp_packet_v4 packet;
	EXPECT_THROW(packet.server_host_name(std::string(64, 'x')), std::logic_error);
	EXPECT_THROW(packet.boot_file_name(std::string(128, 'x')), std::logic_error);
	EXPECT_NO_THROW(packet.boot_file_name(std::string(127, 'x')));
}

TEST(dhcp_packet, truncated_packets_throw)
{
	const std::vector<std::byte> bits(100);
	EXPECT_THROW(dhcp_packet_v4 packet(bits), std::runtime_error);
}
