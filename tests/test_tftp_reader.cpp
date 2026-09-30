#include <vector>

#include <gtest/gtest.h>

#include <tftp_reader.hpp>

#include "test_helpers.hpp"

namespace
{
	auto read_all(std::filesystem::path const& path, std::size_t block_size, std::vector<std::size_t>& block_sizes) -> std::vector<std::byte>
	{
		tftp_reader reader(path, block_size);
		std::vector<std::byte> content;
		while (reader.next())
		{
			block_sizes.push_back(reader.block().size());
			EXPECT_EQ(reader.block_id(), (std::uint16_t)reader.number());
			content.insert(content.end(), reader.block().begin(), reader.block().end());
		}
		return content;
	}
}

TEST(tftp_reader, splits_a_file_into_blocks)
{
	const test::temp_directory dir;
	const auto content = test::pattern(1300);
	const auto path = dir.write("file.bin", content);

	std::vector<std::size_t> sizes;
	EXPECT_EQ(read_all(path, 512, sizes), content);
	EXPECT_EQ(sizes, (std::vector<std::size_t>{ 512, 512, 276 }));
}

TEST(tftp_reader, ends_an_exact_multiple_with_an_empty_block)
{
	const test::temp_directory dir;
	const auto content = test::pattern(1024);
	const auto path = dir.write("file.bin", content);

	std::vector<std::size_t> sizes;
	EXPECT_EQ(read_all(path, 512, sizes), content);
	EXPECT_EQ(sizes, (std::vector<std::size_t>{ 512, 512, 0 }));
}

TEST(tftp_reader, sends_one_empty_block_for_an_empty_file)
{
	const test::temp_directory dir;
	const auto path = dir.write("empty.bin", {});

	std::vector<std::size_t> sizes;
	EXPECT_TRUE(read_all(path, 512, sizes).empty());
	EXPECT_EQ(sizes, (std::vector<std::size_t>{ 0 }));
}

TEST(tftp_reader, builds_data_packets)
{
	const test::temp_directory dir;
	const auto path = dir.write("file.bin", test::pattern(10));
	tftp_reader reader(path, 8);
	ASSERT_TRUE(reader.next());
	const auto packet = reader.data();
	EXPECT_EQ(packet.as<tftp_packet::type_data>().block_id, 1u);
	EXPECT_EQ(packet.as<tftp_packet::type_data>().data.size(), 8u);
	EXPECT_EQ(reader.total_size(), 10u);
}

TEST(tftp_reader, rejects_bad_arguments)
{
	const test::temp_directory dir;
	EXPECT_THROW(tftp_reader(dir.path() / "missing.bin"), std::runtime_error);
	const auto path = dir.write("file.bin", test::pattern(10));
	EXPECT_THROW(tftp_reader(path, 0), std::invalid_argument);
}
