#include <gtest/gtest.h>

#include <common/address_v4.hpp>
#include <common/socket_api.hpp>

TEST(address_v4, parses_address_and_port)
{
	const address_v4 address("10.0.0.1:69");
	EXPECT_EQ(address.addr(), 0x0A000001u);
	EXPECT_EQ(address.port(), 69u);
	EXPECT_EQ(address.to_string(), "10.0.0.1:69");
}

TEST(address_v4, parses_address_without_port)
{
	const address_v4 address("192.168.1.254", 67);
	EXPECT_EQ(address.addr(), 0xC0A801FEu);
	EXPECT_EQ(address.port(), 67u);
}

TEST(address_v4, rejects_invalid_ports)
{
	EXPECT_THROW(address_v4("10.0.0.1:65536"), std::invalid_argument);
	EXPECT_THROW(address_v4("10.0.0.1:12x"), std::invalid_argument);
}

TEST(address_v4, formats_addresses)
{
	EXPECT_EQ(v4_address_to_string(0xFFFFFFFFu), "255.255.255.255");
	EXPECT_EQ(v4_address_to_string(0u), "0.0.0.0");
	EXPECT_EQ(address_v4::everyone(68).to_string(), "255.255.255.255:68");
	EXPECT_EQ(address_v4::loopback(1).to_string(), "127.0.0.1:1");
}

TEST(address_v4, compares)
{
	EXPECT_EQ(address_v4("1.2.3.4:5"), address_v4(0x01020304u, 5));
	EXPECT_NE(address_v4("1.2.3.4:5"), address_v4("1.2.3.4:6"));
}

TEST(mac_address, formats_as_dash_separated_hex)
{
	const std::uint8_t mac[] = { 0x00, 0x1c, 0x7e, 0x35, 0xed, 0x20 };
	EXPECT_EQ(mac_address_to_string(mac), "00-1C-7E-35-ED-20");
	EXPECT_THROW(mac_address_to_string({}), std::runtime_error);
}
