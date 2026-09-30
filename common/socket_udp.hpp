#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "address_v4.hpp"
#include "async.hpp"
#include "serdes.hpp"
#include "socket_api.hpp"

struct datagram
{
	address_v4 source;
	std::vector<std::byte> data;
};

// A non-blocking UDP socket whose receive operations are awaitable.
struct socket_udp
{
	using time_point = async::io_context::time_point;

	socket_udp();
	explicit socket_udp(address_v4 const& bind_address);
	socket_udp(socket_udp&& from) noexcept;
	auto operator = (socket_udp&& from) noexcept -> socket_udp&;
	socket_udp(const socket_udp&) = delete;
	auto operator = (const socket_udp&) -> socket_udp& = delete;
 ~socket_udp();

	void swap(socket_udp& other) noexcept;
	void close() noexcept;

	auto is_open() const noexcept -> bool;
	auto native_handle() const noexcept -> int_socket_type;
	auto local_address() const -> address_v4;

	// Returns a pending datagram, or std::nullopt if there is none.
	auto try_recv() const -> std::optional<datagram>;

	// Waits for a datagram. Returns std::nullopt if `deadline` passes first.
	// Throws async::operation_cancelled when the context is stopped.
	auto async_recv(async::io_context& context, time_point deadline = time_point::max()) const
		-> async::task<std::optional<datagram>>;

	template <typename Rep, typename Period>
	auto async_recv(async::io_context& context, std::chrono::duration<Rep, Period> timeout) const
		-> async::task<std::optional<datagram>>
	{
		const auto duration_v = std::chrono::duration_cast<async::io_context::duration>(timeout);
		return async_recv(context, async::io_context::clock::now() + duration_v);
	}

	auto send(std::span<const std::byte> buffer, const address_v4& target) const -> std::size_t;

	template <typename T>
	requires requires (T const& packet, ::serdes<serdes_writer>& s)
	{
		{ packet.serdes_size_hint() } -> std::convertible_to<std::size_t>;
		{ packet.serdes(s) } -> std::convertible_to<::serdes<serdes_writer>&>;
	}
	auto send(T const& packet, const address_v4& target) const -> std::size_t
	{
		const auto buffer_v = serialize_to_vector(packet);
		return send(std::span<const std::byte>{ buffer_v }, target);
	}

	template <typename O>
	auto option(const typename O::value_type& value) const -> void
	{
		socket_option<O>(m_sock, value);
	}

	template <typename O>
	auto option() const -> typename O::value_type
	{
		return socket_option<O>(m_sock);
	}

private:
	int_socket_type m_sock;
};
