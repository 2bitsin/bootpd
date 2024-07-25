#include "address_v4.hpp"
#include "socket_api.hpp"
#include "socket_udp.hpp"
#include "serdes.hpp"

#include <vector>
#include <array>

using std::exchange;

socket_udp::socket_udp()
:	socket_udp(v4_socket_make_invalid())
{}

socket_udp::socket_udp(int_socket_type int_sock)
:	m_sock{ int_sock }
{}

socket_udp::socket_udp(const struct address_v4& addr)
:	socket_udp(v4_socket_make_udp(addr))
{}

socket_udp::socket_udp(socket_udp&& from)
:	m_sock{ exchange(from.m_sock, v4_socket_make_invalid()) }
{}

auto socket_udp::operator = (socket_udp&& from) -> socket_udp&
{
	socket_udp tmp(std::move(from));
	tmp.swap(*this);
	return *this;
}

socket_udp::~socket_udp()
{
	if (m_sock != v4_socket_make_invalid())
		v4_socket_close(m_sock);
}

void socket_udp::swap(socket_udp& other)
{
	std::swap(other.m_sock, m_sock);
}

void socket_udp::bind(const address_v4& addr)
{
	v4_socket_bind(m_sock, addr);
}

auto socket_udp::recv(std::span<std::byte>& buffer, address_v4& source, uint32_t flags) const -> std::size_t
{
	return v4_socket_recv(m_sock, buffer, source, flags);
}

auto socket_udp::send(std::span<const std::byte>& buffer, const address_v4& target, uint32_t flags) const -> std::size_t
{	
	return v4_socket_send(m_sock, buffer, target, flags);
}

auto socket_udp::recv(uint32_t flags) const -> std::tuple<address_v4, std::vector<std::byte>>
{
  thread_local std::array<std::byte, 0x10000u> array_buffer;
	std::span<std::byte> buffer_s{ array_buffer };
	address_v4 source;
	if ((*this).recv(buffer_s, source, flags) > 0u)
	{
		std::vector<std::byte> received_bytes(buffer_s.begin(), buffer_s.end());
		return std::tuple(std::move(source), std::move(received_bytes));
	}		
	throw std::runtime_error("recv failed");
}
