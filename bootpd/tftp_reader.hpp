#pragma once

#include <cstdint>
#include <cstddef>
#include <fstream>
#include <filesystem>

#include "tftp_packet.hpp"

struct tftp_reader
{	
	tftp_reader(std::filesystem::path path, std::uintmax_t length = 0u, std::uintmax_t block_size = 512u, bool is_binary = true);
	auto data() -> tftp_packet;
	auto next() -> tftp_reader&;
	auto size() const noexcept -> std::uintmax_t;	
	auto number() const noexcept -> std::uintmax_t;
	auto last() const noexcept -> bool;
	auto total_size() const noexcept -> std::uintmax_t;
		
private:
	std::ifstream  m_stream;
	std::uintmax_t m_length;
	std::uintmax_t m_blksiz;
	std::vector<std::byte> m_buffer;
	std::uintmax_t m_number;
};