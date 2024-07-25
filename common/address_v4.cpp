#include "address_v4.hpp"

address_v4::address_v4()
: address_v4(0)
{}

address_v4::address_v4(uint16_t port)
: address_v4(0, port)
{}

address_v4::address_v4(uint32_t address, uint16_t port)
:	m_addr{ address },
	m_port{ port }
{}

address_v4::address_v4(std::pair<uint32_t, uint16_t> const& address_port_pair)
: address_v4(address_port_pair.first, address_port_pair.second)
{}

address_v4::address_v4(std::string_view address, uint16_t port)
: address_v4(v4_parse_address(address), port)
{}

address_v4::address_v4(std::string_view address_and_port)
: address_v4(v4_parse_address_and_port(address_and_port))
{}

auto address_v4::port() const noexcept -> uint16_t { return m_port; }

auto address_v4::addr() const noexcept -> uint32_t { return m_addr; }

auto address_v4::net_port() const noexcept -> uint16_t { return host_to_net<uint16_t>(m_port); }

auto address_v4::net_addr() const noexcept -> uint32_t { return host_to_net<uint32_t>(m_addr); }

auto address_v4::to_string() const noexcept -> std::string
{
	using namespace std::string_literals;
	return v4_address_to_string(m_addr) + ":"s + std::to_string(m_port);
}

auto address_v4::make_udp() const -> socket_udp
{
	return socket_udp(*this);
}

auto address_v4::any(std::uint16_t port) -> address_v4
{
	return address_v4(0x00000000, port);
}

auto address_v4::everyone(std::uint16_t port) -> address_v4
{
	return address_v4(0xffffffff, port);
}

auto address_v4::port(std::uint16_t value) noexcept -> address_v4& 
{ 
	m_port = value;
	return *this; 
}

auto address_v4::addr(std::uint32_t value) noexcept -> address_v4& 
{ 
	m_addr = value;
	return *this; 
}

auto address_v4::port(std::uint16_t value) const noexcept -> address_v4
{ 
	return address_v4(m_addr, value);
}

auto address_v4::addr(std::uint32_t value) const noexcept -> address_v4 
{ 
	return address_v4(value, m_port);	
}