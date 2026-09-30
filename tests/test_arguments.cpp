#include <gtest/gtest.h>

#include <common/arguments.hpp>

using namespace std::string_view_literals;

TEST(arguments, parses_flags_and_values)
{
	const arguments args({ "bootpd"sv, "-C"sv, "my.ini"sv, "-O"sv, "a=1"sv, "b=2"sv, "-v"sv });
	EXPECT_TRUE(args.has("-C"sv));
	EXPECT_TRUE(args.has("-v"sv));
	EXPECT_TRUE(args.has("-x"sv, "-v"sv));
	EXPECT_FALSE(args.has("-q"sv));
	EXPECT_EQ(args.value_or("-C"sv, "config.ini"sv), "my.ini");
	EXPECT_EQ(args.value_or("-D"sv, "default"sv), "default");
	ASSERT_EQ(args.values("-O"sv).size(), 2u);
	EXPECT_EQ(args.values("-O"sv)[1], "b=2");
}

TEST(arguments, values_of_a_missing_flag_is_empty)
{
	const arguments args({ "bootpd"sv, "-O"sv });
	EXPECT_TRUE(args.has("-O"sv));
	EXPECT_TRUE(args.values("-O"sv).empty());
	EXPECT_TRUE(args.values("-Z"sv).empty());
}
