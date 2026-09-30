#include <cstdint>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include <dhcp_options_v4.hpp>

namespace
{
	auto serialize(dhcp_options_v4 const& options) -> std::vector<std::byte>
	{
		return serialize_to_vector(options);
	}

	auto parse(std::vector<std::byte> const& bits) -> dhcp_options_v4
	{
		dhcp_options_v4 options;
		::serdes<serdes_reader> reader(bits);
		reader(options);
		return options;
	}

	auto with_cookie(std::initializer_list<int> tail) -> std::vector<std::byte>
	{
		std::vector<std::byte> bits{ std::byte{0x63}, std::byte{0x82}, std::byte{0x53}, std::byte{0x63} };
		for (auto value : tail)
			bits.push_back((std::byte)value);
		return bits;
	}
}

TEST(dhcp_options, stores_integers_in_network_order)
{
	dhcp_options_v4 options;
	ASSERT_TRUE(options.set(DHCP_OPTION_LEASE_TIME, std::uint32_t(0x01020304)));
	const auto value = options[DHCP_OPTION_LEASE_TIME];
	EXPECT_EQ(std::vector<std::uint8_t>(value.begin(), value.end()), (std::vector<std::uint8_t>{ 1, 2, 3, 4 }));

	std::uint32_t decoded = 0;
	EXPECT_TRUE(options.value(DHCP_OPTION_LEASE_TIME, std::tie(decoded)));
	EXPECT_EQ(decoded, 0x01020304u);
}

TEST(dhcp_options, stores_strings_with_their_real_length)
{
	// Regression : strings used to be stored as sizeof(std::string) bytes.
	dhcp_options_v4 options;
	ASSERT_TRUE(options.set(DHCP_OPTION_BOOT_FILE_NAME, std::string("pxelinux.0")));
	EXPECT_EQ(options[DHCP_OPTION_BOOT_FILE_NAME].size(), 10u);

	const std::string long_name(200, 'x');
	EXPECT_TRUE(options.set(DHCP_OPTION_DOMAIN_NAME, long_name));
	EXPECT_EQ(options[DHCP_OPTION_DOMAIN_NAME].size(), 200u);
	EXPECT_FALSE(options.set(DHCP_OPTION_DOMAIN_NAME, std::string(256, 'x')));
}

TEST(dhcp_options, rejects_pad_and_end_codes)
{
	dhcp_options_v4 options;
	EXPECT_FALSE(options.set(DHCP_OPTION_PAD, std::uint8_t(1)));
	EXPECT_FALSE(options.set(DHCP_OPTION_END, std::uint8_t(1)));
	EXPECT_FALSE(options.has(DHCP_OPTION_PAD));
	EXPECT_TRUE(options[DHCP_OPTION_END].empty());
}

TEST(dhcp_options, assign_ignores_invalid_codes)
{
	// Regression : a parameter request list containing 0 or 255 used to index
	// out of bounds.
	dhcp_options_v4 from;
	from.set(DHCP_OPTION_SUBNET_MASK, std::uint32_t(0xffffff00));
	dhcp_options_v4 to;
	EXPECT_FALSE(to.assign(0, from));
	EXPECT_FALSE(to.assign(255, from));
	EXPECT_FALSE(to.assign(DHCP_OPTION_ROUTER, from));
	EXPECT_TRUE(to.assign(DHCP_OPTION_SUBNET_MASK, from));
	EXPECT_EQ(to[DHCP_OPTION_SUBNET_MASK].size(), 4u);
}

TEST(dhcp_options, round_trips)
{
	dhcp_options_v4 options;
	options.message_type(DHCP_MESSAGE_TYPE_OFFER);
	options.set(DHCP_OPTION_SUBNET_MASK, std::uint32_t(0xff000000));
	options.set(DHCP_OPTION_HOST_NAME, std::string("host"));
	const auto bits = serialize(options);
	EXPECT_EQ(bits.size(), options.serdes_size_hint());
	EXPECT_EQ(bits.back(), std::byte{ DHCP_OPTION_END });

	const auto parsed = parse(bits);
	EXPECT_EQ(parsed.message_type(), DHCP_MESSAGE_TYPE_OFFER);
	EXPECT_EQ(parsed[DHCP_OPTION_HOST_NAME].size(), 4u);
	EXPECT_TRUE(parsed.has_cookie());
}

TEST(dhcp_options, parses_pad_and_tolerates_missing_end)
{
	const auto parsed = parse(with_cookie({ 0, 0, 53, 1, 3, 55, 2, 1, 3 }));
	EXPECT_EQ(parsed.message_type(), DHCP_MESSAGE_TYPE_REQUEST);
	EXPECT_EQ(parsed.requested_parameters().size(), 2u);
}

TEST(dhcp_options, ignores_truncated_options)
{
	const auto parsed = parse(with_cookie({ 53, 1, 1, 12, 10, 'a', 'b' }));
	EXPECT_EQ(parsed.message_type(), DHCP_MESSAGE_TYPE_DISCOVER);
	EXPECT_FALSE(parsed.has(DHCP_OPTION_HOST_NAME));
}

TEST(dhcp_options, concatenates_repeated_options)
{
	const auto parsed = parse(with_cookie({ 12, 2, 'a', 'b', 12, 1, 'c', 255 }));
	EXPECT_EQ(parsed[DHCP_OPTION_HOST_NAME].size(), 3u);
}

TEST(dhcp_options, without_cookie_there_are_no_options)
{
	const auto parsed = parse(std::vector<std::byte>(8, std::byte{ 0x35 }));
	EXPECT_FALSE(parsed.has_cookie());
	EXPECT_FALSE(parsed.message_type().has_value());
}

TEST(dhcp_options, short_values_do_not_decode)
{
	const auto parsed = parse(with_cookie({ 51, 2, 0, 1, 255 }));
	std::uint32_t lease = 0;
	EXPECT_FALSE(parsed.value(DHCP_OPTION_LEASE_TIME, std::tie(lease)));
}
