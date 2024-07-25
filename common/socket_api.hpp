#pragma once

#include <cstdint>
#include <cstddef>

#include <string_view>
#include <tuple>
#include <span>

#include "socket_option.hpp"
#include "socket_error.hpp"

using int_socket_type = std::intptr_t;

auto mac_address_to_string(std::span<const std::uint8_t> data) -> std::string;

void v4_init_sockaddr(struct sockaddr_in& target, std::size_t len, const struct address_v4& source);
void v4_init_sockaddr(struct sockaddr& target, std::size_t len, const struct address_v4& source);
auto v4_parse_address(std::string_view what) -> uint32_t;
auto v4_parse_address_and_port(std::string_view what) -> std::pair<uint32_t, uint16_t>;
auto v4_parse_address_and_port(struct sockaddr_in const& what) -> std::pair<uint32_t, uint16_t>;
auto v4_address_to_string(std::uint32_t) -> std::string;
auto v4_resolve_single(std::string_view target) -> std::uint32_t;
auto v4_socket_make_udp() -> int_socket_type;
auto v4_socket_make_udp(const struct address_v4& address) -> int_socket_type;
auto v4_socket_make_invalid() -> int_socket_type;
void v4_socket_bind(int_socket_type socket, const struct address_v4& address);
void v4_socket_close(int_socket_type socket);
auto v4_socket_recv(int_socket_type socket, std::span<std::byte>& buffer, struct address_v4& address, std::uint32_t flags) -> std::size_t;
auto v4_socket_send(int_socket_type socket, std::span<const std::byte>& buffer, const struct address_v4& address, std::uint32_t flags) -> std::size_t;

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
	typename O::value_type value;
	socket_option_get(target, O::level(), O::option(), &value, sizeof(value));
	return value;	
}

