#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "tftp_packet.hpp"

// Reads a file sequentially, one TFTP block at a time.
//
// A transfer always ends with a block shorter than the block size, so a file
// whose size is a multiple of the block size ends with an empty block.
struct tftp_reader
{
	// Throws std::runtime_error if the file can't be opened.
	tftp_reader(std::filesystem::path const& path, std::size_t block_size = 512u);

	// Loads the next block, returns false once the last block was loaded.
	auto next() -> bool;

	// DATA packet for the current block (block numbers wrap around after 65535).
	auto data() const -> tftp_packet;
	auto block() const noexcept -> std::span<const std::byte>;
	auto number() const noexcept -> std::uintmax_t;
	auto block_id() const noexcept -> std::uint16_t;
	auto last() const noexcept -> bool;
	auto total_size() const noexcept -> std::uintmax_t;

private:
	std::ifstream          m_stream;
	std::uintmax_t         m_length{ 0u };
	std::size_t            m_blksiz{ 512u };
	std::vector<std::byte> m_buffer;
	std::uintmax_t         m_number{ 0u };
	bool                   m_last{ false };
};
