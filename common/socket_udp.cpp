#include "socket_udp.hpp"

#include <array>

socket_udp::socket_udp()
:	m_sock{ v4_socket_make_invalid() }
{}

socket_udp::socket_udp(address_v4 const& bind_address)
:	m_sock{ v4_socket_make_udp(bind_address) }
{}

socket_udp::socket_udp(socket_udp&& from) noexcept
:	m_sock{ std::exchange(from.m_sock, v4_socket_make_invalid()) }
{}

auto socket_udp::operator = (socket_udp&& from) noexcept -> socket_udp&
{
	socket_udp tmp(std::move(from));
	tmp.swap(*this);
	return *this;
}

socket_udp::~socket_udp()
{
	close();
}

void socket_udp::swap(socket_udp& other) noexcept
{
	std::swap(other.m_sock, m_sock);
}

void socket_udp::close() noexcept
{
	if (is_open())
		v4_socket_close(std::exchange(m_sock, v4_socket_make_invalid()));
}

auto socket_udp::is_open() const noexcept -> bool
{
	return m_sock != v4_socket_make_invalid();
}

auto socket_udp::native_handle() const noexcept -> int_socket_type
{
	return m_sock;
}

auto socket_udp::local_address() const -> address_v4
{
	return v4_socket_local_address(m_sock);
}

auto socket_udp::try_recv() const -> std::optional<datagram>
{
	// Everything runs on the io_context thread, one buffer per thread suffices.
	thread_local std::array<std::byte, 0x10000u> array_buffer;
	std::span<std::byte> buffer_s{ array_buffer };
	address_v4 source;
	if (!v4_socket_recv(m_sock, buffer_s, source))
		return std::nullopt;
	return datagram{ source, std::vector<std::byte>(buffer_s.begin(), buffer_s.end()) };
}

auto socket_udp::async_recv(async::io_context& context, time_point deadline) const
	-> async::task<std::optional<datagram>>
{
	for (;;)
	{
		if (auto datagram_v = try_recv(); datagram_v.has_value())
			co_return datagram_v;
		if (!co_await context.wait_readable(m_sock, deadline))
			co_return std::nullopt;
	}
}

auto socket_udp::send(std::span<const std::byte> buffer, const address_v4& target) const -> std::size_t
{
	return v4_socket_send(m_sock, buffer, target);
}
