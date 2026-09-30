#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <tftp_packet.hpp>

#include "test_helpers.hpp"

namespace
{
	auto round_trip(tftp_packet const& packet) -> tftp_packet
	{
		return tftp_packet(serialize_to_vector(packet));
	}
}

TEST(tftp_packet, parses_read_requests_with_options)
{
	const auto bits = test::bytes(std::string_view("\0\1boot.bin\0OCTET\0BlkSize\0" "1024\0tsize\0" "0\0", 38));
	const tftp_packet packet(bits);
	ASSERT_TRUE(packet.is<tftp_packet::type_rrq>());
	const auto& rrq = packet.as<tftp_packet::type_rrq>();
	EXPECT_EQ(rrq.filename, "boot.bin");
	EXPECT_EQ(rrq.xfermode, "octet");        // mode is case insensitive
	EXPECT_EQ(rrq.options.at("blksize"), "1024"); // so are option names
	EXPECT_EQ(rrq.options.at("tsize"), "0");
}

TEST(tftp_packet, round_trips_every_packet_type)
{
	const auto rrq = round_trip(tftp_packet::make_rrq("a", "octet", { { "blksize", "8" } }));
	EXPECT_EQ(rrq.as<tftp_packet::type_rrq>().options.at("blksize"), "8");

	const auto wrq = round_trip(tftp_packet::make_wrq("b", "netascii", {}));
	EXPECT_EQ(wrq.as<tftp_packet::type_wrq>().filename, "b");

	const auto payload = test::pattern(100);
	const auto data = round_trip(tftp_packet::make_data(7, payload));
	EXPECT_EQ(data.as<tftp_packet::type_data>().block_id, 7u);
	EXPECT_EQ(data.as<tftp_packet::type_data>().data, payload);

	const auto empty = round_trip(tftp_packet::make_data(8, {}));
	EXPECT_TRUE(empty.as<tftp_packet::type_data>().data.empty());

	const auto ack = round_trip(tftp_packet::make_ack(65535));
	EXPECT_EQ(ack.as<tftp_packet::type_ack>().block_id, 65535u);

	const auto error = round_trip(tftp_packet::make_error(tftp_packet::file_not_found));
	EXPECT_EQ(error.as<tftp_packet::type_error>().error_code, tftp_packet::file_not_found);
	EXPECT_EQ(error.as<tftp_packet::type_error>().error_string, "File not found");

	const auto oack = round_trip(tftp_packet::make_oack({ { "tsize", "123" } }));
	EXPECT_EQ(oack.as<tftp_packet::type_oack>().options.at("tsize"), "123");
}

TEST(tftp_packet, has_the_rfc_wire_format)
{
	const auto bits = serialize_to_vector(tftp_packet::make_ack(0x0102));
	EXPECT_EQ(bits, test::bytes({ 0, 4, 1, 2 }));
	EXPECT_EQ(tftp_packet::make_ack(1).opcode(), 4u);
}

TEST(tftp_packet, accepts_error_messages_without_terminator)
{
	const tftp_packet packet(test::bytes({ 0, 5, 0, 1, 'o', 'o', 'p', 's' }));
	EXPECT_EQ(packet.as<tftp_packet::type_error>().error_string, "oops");
}

TEST(tftp_packet, rejects_malformed_packets)
{
	EXPECT_THROW(tftp_packet(test::bytes({ 0 })), std::runtime_error);
	EXPECT_THROW(tftp_packet(test::bytes({ 0, 9 })), std::runtime_error);
	EXPECT_THROW(tftp_packet(test::bytes({ 0, 1, 'a', 'b' })), std::runtime_error);
	EXPECT_THROW(tftp_packet(test::bytes({ 0, 4, 1 })), std::runtime_error);
}

TEST(tftp_packet, formats_as_text)
{
	EXPECT_EQ(tftp_packet::make_ack(3).to_string(), "ACK(block_id=3)");
	EXPECT_EQ(tftp_packet::make_oack({ { "a", "1" }, { "b", "2" } }).to_string(), "OACK(a=\"1\", b=\"2\")");
	EXPECT_EQ(tftp_packet().to_string(), "(Nil)");
	EXPECT_THROW(tftp_packet().opcode(), std::logic_error);
}
