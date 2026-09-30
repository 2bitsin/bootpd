#include <cstdint>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include <common/lexical_cast.hpp>

TEST(lexical_cast, parses_decimal_integers)
{
	EXPECT_EQ(lexical_cast<int>("42"), 42);
	EXPECT_EQ(lexical_cast<int>("-42"), -42);
	EXPECT_EQ(lexical_cast<int>("+7"), 7);
	EXPECT_EQ(lexical_cast<std::uint32_t>("172800"), 172800u);
}

TEST(lexical_cast, parses_prefixed_integers)
{
	EXPECT_EQ(lexical_cast<int>("0x1F"), 31);
	EXPECT_EQ(lexical_cast<int>("0X1f"), 31);
	EXPECT_EQ(lexical_cast<int>("0o17"), 15);
	EXPECT_EQ(lexical_cast<int>("0b101"), 5);
	EXPECT_EQ(lexical_cast<int>("0B101"), 5);
}

TEST(lexical_cast, checks_integer_range)
{
	EXPECT_EQ(lexical_cast<std::uint16_t>("65535"), 65535u);
	EXPECT_THROW(lexical_cast<std::uint16_t>("65536"), bad_lexical_cast);
	EXPECT_THROW(lexical_cast<std::uint16_t>("-1"), bad_lexical_cast);
	EXPECT_EQ(lexical_cast<std::int8_t>("-128"), -128);
	EXPECT_THROW(lexical_cast<std::int8_t>("128"), bad_lexical_cast);
	EXPECT_THROW(lexical_cast<std::int8_t>("-129"), bad_lexical_cast);
	EXPECT_EQ(lexical_cast<int>("-0"), 0);
}

TEST(lexical_cast, rejects_malformed_integers)
{
	EXPECT_THROW(lexical_cast<int>(""), bad_lexical_cast);
	EXPECT_THROW(lexical_cast<int>("12abc"), bad_lexical_cast);
	EXPECT_THROW(lexical_cast<int>("0x"), bad_lexical_cast);
	EXPECT_THROW(lexical_cast<int>("abc"), bad_lexical_cast);
	EXPECT_THROW(lexical_cast<int>(" 1"), bad_lexical_cast);
}

TEST(lexical_cast, parses_booleans)
{
	for (auto text : { "true", "True", "TRUE", "yes", "on", "1" })
		EXPECT_TRUE(lexical_cast<bool>(text)) << text;
	for (auto text : { "false", "False", "FALSE", "no", "off", "0" })
		EXPECT_FALSE(lexical_cast<bool>(text)) << text;
	EXPECT_THROW(lexical_cast<bool>("maybe"), bad_lexical_cast);
}

TEST(lexical_cast, parses_floating_point)
{
	EXPECT_DOUBLE_EQ(lexical_cast<double>("1.5"), 1.5);
	EXPECT_THROW(lexical_cast<double>("1.5x"), bad_lexical_cast);
}

TEST(lexical_cast, constructs_other_types)
{
	EXPECT_EQ(lexical_cast<std::string>("hello"), "hello");
	EXPECT_EQ(lexical_cast<std::filesystem::path>("a/b"), std::filesystem::path("a/b"));
}
