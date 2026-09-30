#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

// An IPv4 address and UDP port, both stored in host byte order.
struct address_v4
{
	address_v4();
	address_v4(std::uint16_t port);
	address_v4(std::uint32_t address, std::uint16_t port);
	address_v4(std::pair<std::uint32_t, std::uint16_t> const& address_port_pair);
	address_v4(std::string_view address, std::uint16_t port);
	// Accepts "a.b.c.d", "a.b.c.d:port", "hostname" or "hostname:port".
	address_v4(std::string_view address_and_port);

	auto port() const noexcept -> std::uint16_t;
	auto addr() const noexcept -> std::uint32_t;
	auto net_port() const noexcept -> std::uint16_t;
	auto net_addr() const noexcept -> std::uint32_t;

	auto port(std::uint16_t value) noexcept -> address_v4&;
	auto addr(std::uint32_t value) noexcept -> address_v4&;
	auto port(std::uint16_t value) const noexcept -> address_v4;
	auto addr(std::uint32_t value) const noexcept -> address_v4;

	auto to_string() const -> std::string;
	static auto any(std::uint16_t port = 0) -> address_v4;
	static auto everyone(std::uint16_t port = 0) -> address_v4;
	static auto loopback(std::uint16_t port = 0) -> address_v4;

	auto operator <=> (address_v4 const& lhs) const noexcept -> std::strong_ordering = default;

private:
	std::uint32_t m_addr{ 0 };
	std::uint16_t m_port{ 0 };
};
