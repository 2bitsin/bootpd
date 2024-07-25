#include "tftp_reader.hpp"

tftp_reader::tftp_reader(std::filesystem::path path, std::uintmax_t length, std::uintmax_t blksiz,  bool is_binary):
	m_stream (path, is_binary ? std::ios::binary : 0u),
	m_length (length ? length : std::filesystem::file_size(path)),
	m_blksiz (blksiz),
	m_buffer (blksiz),
	m_number (0u)
{ next(); }

auto tftp_reader::data() -> tftp_packet
{
	return tftp_packet::make_data((m_number & 0xffffu), m_buffer);
}

auto tftp_reader::next() -> tftp_reader&
{
	const auto offset = m_number*m_blksiz;
	const auto length = std::min(m_length - offset, m_blksiz);
	m_buffer.clear();
	if (length > 0)
	{
		m_buffer.resize(length);
		m_stream.seekg(offset, std::ios::beg);		
		m_stream.read((char*)m_buffer.data(), length);
	}
	++m_number;
	return *this;
}

auto tftp_reader::size() const noexcept -> std::uintmax_t
{
	return m_buffer.size();
}

auto tftp_reader::number() const noexcept -> std::uintmax_t
{
	return m_number;
}

auto tftp_reader::last() const noexcept -> bool
{
	return m_buffer.size() < m_blksiz;
}

auto tftp_reader::total_size() const noexcept -> std::uintmax_t
{
	return m_length;
}



