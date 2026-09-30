#pragma once

// Thin, platform neutral wrapper around the BSD socket / Winsock API.
// All platform specific headers are confined to socket_api.cpp.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "socket_option.hpp"

using int_socket_type = std::intptr_t;

struct address_v4;

auto mac_address_to_string(std::span<const std::uint8_t> data) -> std::string;

auto v4_parse_address(std::string_view what) -> std::uint32_t;
auto v4_parse_address_and_port(std::string_view what) -> std::pair<std::uint32_t, std::uint16_t>;
auto v4_address_to_string(std::uint32_t address) -> std::string;
auto v4_resolve_single(std::string_view target) -> std::uint32_t;

auto v4_socket_make_invalid() -> int_socket_type;
auto v4_socket_make_udp() -> int_socket_type;
// Creates a non-blocking UDP socket bound to `address`.
auto v4_socket_make_udp(const address_v4& address) -> int_socket_type;
void v4_socket_bind(int_socket_type socket, const address_v4& address);
void v4_socket_close(int_socket_type socket);
void v4_socket_set_nonblocking(int_socket_type socket);
auto v4_socket_local_address(int_socket_type socket) -> address_v4;

// Non-blocking receive. Returns false if no datagram is pending, otherwise
// shrinks `buffer` to the received bytes and stores the sender in `address`.
auto v4_socket_recv(int_socket_type socket, std::span<std::byte>& buffer, address_v4& address) -> bool;
auto v4_socket_send(int_socket_type socket, std::span<const std::byte> buffer, const address_v4& address) -> std::size_t;

struct socket_poll_entry
{
	int_socket_type socket;
	bool readable{ false };
};

// Waits until at least one socket is readable or the timeout expires.
void v4_socket_poll(std::span<socket_poll_entry> entries, int timeout_ms);

namespace detail
{
	void socket_option_set(int_socket_type target, int level, int option, const void* value, int size);
	void socket_option_get(int_socket_type target, int level, int option, void* value, int size);
}

template <typename O>
void socket_option(int_socket_type target, typename O::value_type const& value)
{
	using namespace detail;
	socket_option_set(target, O::level(), O::option(), &value, sizeof(value));
}

template <typename O>
auto socket_option(int_socket_type target) -> typename O::value_type
{
	using namespace detail;
	typename O::value_type value{};
	socket_option_get(target, O::level(), O::option(), &value, sizeof(value));
	return value;
}
