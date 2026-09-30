#include <cstring>
#include <array>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include <common/byte_order.hpp>
#include <common/serdes.hpp>

TEST(byte_order, converts_to_network_order)
{
	const std::uint32_t value = 0x01020304u;
	const auto net_v = host_to_net(value);
	std::array<std::uint8_t, 4> bytes_v{};
	std::memcpy(bytes_v.data(), &net_v, sizeof(net_v));
	EXPECT_EQ(bytes_v, (std::array<std::uint8_t, 4>{ 1, 2, 3, 4 }));
	EXPECT_EQ(net_to_host(net_v), value);
}

TEST(serdes, round_trips_scalars_in_network_order)
{
	std::vector<std::byte> buffer_v(7);
	::serdes<serdes_writer> writer(buffer_v);
	writer(std::uint8_t(0xAB));
	writer(std::uint16_t(0x0102));
	writer(std::uint32_t(0x03040506));
	EXPECT_EQ(writer.consumed_bytes(), 7u);
	EXPECT_EQ(buffer_v[1], std::byte{ 0x01 });
	EXPECT_EQ(buffer_v[6], std::byte{ 0x06 });

	::serdes<serdes_reader> reader(buffer_v);
	std::uint8_t a{};
	std::uint16_t b{};
	std::uint32_t c{};
	reader(a);
	reader(b);
	reader(c);
	EXPECT_EQ(a, 0xABu);
	EXPECT_EQ(b, 0x0102u);
	EXPECT_EQ(c, 0x03040506u);
	EXPECT_TRUE(reader.empty());
}

TEST(serdes, reads_unaligned_values)
{
	std::vector<std::byte> buffer_v{ std::byte{0}, std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0x78} };
	::serdes<serdes_reader> reader(buffer_v);
	reader.skip(1);
	std::uint32_t value{};
	reader(value);
	EXPECT_EQ(value, 0x12345678u);
}

TEST(serdes, throws_on_overrun)
{
	std::vector<std::byte> buffer_v(3);
	::serdes<serdes_reader> reader(buffer_v);
	std::uint32_t value{};
	EXPECT_THROW(reader(value), std::runtime_error);

	::serdes<serdes_writer> writer(buffer_v);
	EXPECT_THROW(writer(std::uint32_t(1)), std::runtime_error);
}

TEST(serdes, round_trips_asciiz_strings)
{
	std::vector<std::byte> buffer_v(16);
	::serdes<serdes_writer> writer(buffer_v);
	writer(std::string("abc"), serdes_asciiz);
	writer(std::string(""), serdes_asciiz);
	EXPECT_EQ(writer.consumed_bytes(), 5u);

	::serdes<serdes_reader> reader(std::span<const std::byte>(buffer_v.data(), 5));
	std::string first, second;
	reader(first, serdes_asciiz);
	reader(second, serdes_asciiz);
	EXPECT_EQ(first, "abc");
	EXPECT_EQ(second, "");
}

TEST(serdes, unterminated_asciiz_string_throws)
{
	std::vector<std::byte> buffer_v{ std::byte{'a'}, std::byte{'b'} };
	::serdes<serdes_reader> reader(buffer_v);
	std::string value;
	EXPECT_THROW(reader(value, serdes_asciiz), std::runtime_error);
}
