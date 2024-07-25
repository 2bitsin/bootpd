#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <string>
#include <compare>

#include "common/byte_order.hpp"
#include "socket_api.hpp"
#include "socket_udp.hpp"

struct address_v4
{
	address_v4();
	address_v4(uint16_t port);
	address_v4(uint32_t address, uint16_t port);
	address_v4(std::pair<uint32_t, uint16_t> const& address_port_pair);
	address_v4(std::string_view address, uint16_t port);	
	address_v4(std::string_view address_and_port);
	
	auto port() const noexcept -> uint16_t;
	auto addr() const noexcept -> uint32_t;	
	auto net_port() const noexcept -> uint16_t;
	auto net_addr() const noexcept -> uint32_t;

	auto port(std::uint16_t value) noexcept -> address_v4&; 
	auto addr(std::uint32_t value) noexcept -> address_v4&; 
	auto port(std::uint16_t value) const noexcept -> address_v4;
	auto addr(std::uint32_t value) const noexcept -> address_v4; 
	
	template <typename T>
	auto assign_to(T& s) const
	{
		v4_init_sockaddr(s, sizeof(s), *this);
	}	
	
	template <typename T>
	auto assign_from(T const& source)
	{
		std::tie(m_addr, m_port) = v4_parse_address_and_port(source);
	}

	template <typename T>
	auto as() const -> T
	{
		T target;
		assign_to(target);
		return target;
	}

	auto to_string() const noexcept -> std::string;
	auto make_udp() const -> socket_udp;
	static auto any(std::uint16_t port = 0) -> address_v4;
	static auto everyone(std::uint16_t port = 0) -> address_v4;

	auto operator <=> (address_v4 const& lhs) const noexcept -> std::strong_ordering = default;	
	
private:
	uint32_t m_addr{ 0 } ;
	uint16_t m_port{ 0 } ;
};