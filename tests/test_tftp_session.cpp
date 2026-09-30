#include <filesystem>

#include <gtest/gtest.h>

#include <tftp_session_v4.hpp>

#include "test_helpers.hpp"

using session = tftp_session_v4;

TEST(tftp_negotiation, without_options_there_is_no_oack)
{
	const auto result = session::negotiate({}, 1000);
	EXPECT_TRUE(result.oack.empty());
	EXPECT_EQ(result.options.blksize, 512u);
	EXPECT_EQ(result.options.timeout, 1u);
}

TEST(tftp_negotiation, accepts_valid_options)
{
	const auto result = session::negotiate({ { "blksize", "1428" }, { "timeout", "3" }, { "tsize", "0" } }, 123456);
	EXPECT_EQ(result.options.blksize, 1428u);
	EXPECT_EQ(result.options.timeout, 3u);
	EXPECT_EQ(result.oack.at("blksize"), "1428");
	EXPECT_EQ(result.oack.at("timeout"), "3");
	EXPECT_EQ(result.oack.at("tsize"), "123456");
}

TEST(tftp_negotiation, ignores_invalid_and_unknown_options)
{
	// Regression : blksize=0 made the transfer loop forever, timeout=0 made
	// the session wait forever and junk values killed the session.
	for (auto value : { "0", "7", "65465", "-1", "abc", "" })
	{
		const auto result = session::negotiate({ { "blksize", value } }, 1);
		EXPECT_EQ(result.options.blksize, 512u) << value;
		EXPECT_TRUE(result.oack.empty()) << value;
	}
	for (auto value : { "0", "256", "x" })
	{
		const auto result = session::negotiate({ { "timeout", value } }, 1);
		EXPECT_EQ(result.options.timeout, 1u) << value;
		EXPECT_TRUE(result.oack.empty()) << value;
	}
	EXPECT_TRUE(session::negotiate({ { "windowsize", "4" }, { "multicast", "" } }, 1).oack.empty());
}

TEST(tftp_negotiation, accepts_the_range_limits)
{
	EXPECT_EQ(session::negotiate({ { "blksize", "8" } }, 1).options.blksize, 8u);
	EXPECT_EQ(session::negotiate({ { "blksize", "65464" } }, 1).options.blksize, 65464u);
	EXPECT_EQ(session::negotiate({ { "timeout", "255" } }, 1).options.timeout, 255u);
}

class tftp_resolve_path: public ::testing::Test
{
protected:
	void SetUp() override
	{
		root = dir.path() / "root";
		dir.write("root/boot/pxelinux.0", test::bytes("x"));
		dir.write("secret.txt", test::bytes("secret"));
	}

	auto resolve(std::string_view name) const { return session::resolve_path(root, name); }
	auto expected(std::filesystem::path const& relative) const { return std::filesystem::weakly_canonical(root / relative); }

	test::temp_directory dir;
	std::filesystem::path root;
};

TEST_F(tftp_resolve_path, resolves_names_inside_the_root)
{
	EXPECT_EQ(resolve("boot/pxelinux.0"), expected("boot/pxelinux.0"));
	EXPECT_EQ(resolve("/boot/pxelinux.0"), expected("boot/pxelinux.0"));
	EXPECT_EQ(resolve("boot\\pxelinux.0"), expected("boot/pxelinux.0"));
	EXPECT_EQ(resolve("boot/../boot/./pxelinux.0"), expected("boot/pxelinux.0"));
	EXPECT_EQ(resolve("missing.bin"), expected("missing.bin"));
}

TEST_F(tftp_resolve_path, rejects_names_outside_the_root)
{
	// Regression : "../" and absolute paths gave access to any file.
	EXPECT_FALSE(resolve("../secret.txt").has_value());
	EXPECT_FALSE(resolve("..\\secret.txt").has_value());
	EXPECT_FALSE(resolve("boot/../../secret.txt").has_value());
	EXPECT_FALSE(resolve("").has_value());
	EXPECT_FALSE(resolve("/").has_value());
	EXPECT_FALSE(resolve(".").has_value());
}

TEST_F(tftp_resolve_path, absolute_names_stay_inside_the_root)
{
	const auto outside = (dir.path() / "secret.txt").generic_string();
	const auto resolved = resolve(outside);
	if (resolved.has_value())
	{
		const auto root_v = std::filesystem::weakly_canonical(root);
		EXPECT_EQ(std::mismatch(root_v.begin(), root_v.end(), resolved->begin(), resolved->end()).first, root_v.end());
	}
}

TEST_F(tftp_resolve_path, rejects_symlinks_leading_outside)
{
	std::error_code ec;
	std::filesystem::create_symlink(dir.path() / "secret.txt", root / "link.txt", ec);
	if (ec)
		GTEST_SKIP() << "symlinks not available : " << ec.message();
	EXPECT_FALSE(resolve("link.txt").has_value());
}
