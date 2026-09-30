#include "tftp_reader.hpp"

#include <algorithm>
#include <stdexcept>

tftp_reader::tftp_reader(std::filesystem::path const& path, std::size_t block_size)
:	m_stream (path, std::ios::in | std::ios::binary),
	m_blksiz (block_size)
{
	if (!m_stream.is_open())
		throw std::runtime_error("Unable to open file : " + path.string());
	if (m_blksiz == 0u)
		throw std::invalid_argument("Block size must not be zero.");
	m_length = std::filesystem::file_size(path);
	m_buffer.reserve(m_blksiz);
}

auto tftp_reader::next() -> bool
{
	if (m_last)
		return false;

	const auto offset = m_number * m_blksiz;
	const auto length = (std::size_t)std::min<std::uintmax_t>(m_length - std::min(offset, m_length), m_blksiz);
	m_buffer.resize(length);
	if (length > 0u)
	{
		m_stream.read((char*)m_buffer.data(), (std::streamsize)length);
		if ((std::size_t)m_stream.gcount() != length)
			throw std::runtime_error("Unexpected end of file.");
	}
	++m_number;
	m_last = length < m_blksiz;
	return true;
}

auto tftp_reader::data() const -> tftp_packet
{
	return tftp_packet::make_data(block_id(), m_buffer);
}

auto tftp_reader::block() const noexcept -> std::span<const std::byte>
{
	return m_buffer;
}

auto tftp_reader::number() const noexcept -> std::uintmax_t
{
	return m_number;
}

auto tftp_reader::block_id() const noexcept -> std::uint16_t
{
	return (std::uint16_t)(m_number & 0xffffu);
}

auto tftp_reader::last() const noexcept -> bool
{
	return m_last;
}

auto tftp_reader::total_size() const noexcept -> std::uintmax_t
{
	return m_length;
}
