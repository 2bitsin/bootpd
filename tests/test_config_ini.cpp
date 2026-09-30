#include <gtest/gtest.h>

#include "test_helpers.hpp"

using namespace std::string_view_literals;

TEST(config_ini, reads_global_and_section_values)
{
	const auto cfg = test::config_from(R"(
key1 = value1   ; a comment
key2=value2

[section]
key1 = other
)");
	EXPECT_EQ(cfg.value("key1"sv), "value1");
	EXPECT_EQ(cfg.value("key2"sv), "value2");
	EXPECT_EQ(cfg.value({ "key1"sv, "section"sv }), "other");
	EXPECT_FALSE(cfg.value("missing"sv).has_value());
	EXPECT_TRUE(cfg.has_section("section"));
	EXPECT_FALSE(cfg.has_section("nope"));
}

TEST(config_ini, accepts_empty_values)
{
	// Used to be undefined behaviour (front() of an empty string_view).
	const auto cfg = test::config_from("empty =\nquoted = \"\"\n");
	EXPECT_EQ(cfg.value("empty"sv), "");
	EXPECT_EQ(cfg.value("quoted"sv), "");
}

TEST(config_ini, strips_quotes_and_keeps_quoted_semicolons)
{
	const auto cfg = test::config_from("name = \"a ; b\" ; comment\n");
	EXPECT_EQ(cfg.value("name"sv), "a ; b");
}

TEST(config_ini, handles_crlf_and_byte_order_mark)
{
	const auto cfg = test::config_from("\xEF\xBB\xBFkey = value\r\n[ sect ]\r\nother = 1\r\n");
	EXPECT_EQ(cfg.value("key"sv), "value");
	EXPECT_EQ(cfg.value({ "other"sv, "sect"sv }), "1");
}

TEST(config_ini, reports_the_right_line_number)
{
	try
	{
		test::config_from("a = 1\n\n\nthis line is broken\n");
		FAIL() << "expected an exception";
	}
	catch (std::runtime_error const& ex)
	{
		EXPECT_NE(std::string(ex.what()).find("line nr 4"), std::string::npos) << ex.what();
	}
}

TEST(config_ini, rejects_missing_key_names)
{
	EXPECT_THROW(test::config_from(" = value\n"), std::runtime_error);
}

TEST(config_ini, converts_values)
{
	const auto cfg = test::config_from("port = 69\nflag = yes\nbad = 70000\n");
	EXPECT_EQ(cfg.value_or<std::uint16_t>("port"sv, 1u), 69u);
	EXPECT_EQ(cfg.value_or<std::uint16_t>("missing"sv, 1u), 1u);
	EXPECT_TRUE(cfg.value_or("flag"sv, false));
	// Invalid values are an error, not silently replaced by the default.
	EXPECT_THROW(cfg.value_or<std::uint16_t>("bad"sv, 1u), std::runtime_error);
}

TEST(config_ini, insert_line_overrides_values)
{
	auto cfg = test::config_from("key = old\n");
	cfg.insert_line("key = new");
	EXPECT_EQ(cfg.value("key"sv), "new");
}
