#pragma once 

#include <span>
#include <chrono>
#include <type_traits>
#include <concepts>
#include <utility>

#include "socket_api.hpp"
#include "serdes.hpp"

inline static const constexpr std::uint32_t message_out_of_bounds_flag	= 0x01u;
inline static const constexpr std::uint32_t message_peek_flag						= 0x02u;
inline static const constexpr std::uint32_t message_dont_route_flag			= 0x04u;
inline static const constexpr std::uint32_t message_wait_all_flag				= 0x08u;

struct socket_udp
{
	socket_udp();
	socket_udp(const struct address_v4& addr);
	socket_udp(socket_udp&& from);
	socket_udp& operator = (socket_udp && from);
	socket_udp(const socket_udp&) = delete;
	socket_udp& operator = (const socket_udp&) = delete;
 ~socket_udp();
  void swap(socket_udp& other);
	void bind(const struct address_v4& addr);
	
	/* buffer will be adjusted to span only the bytes received */
	auto recv(std::span<std::byte>& buffer, struct address_v4& source, uint32_t flags) const -> std::size_t;

	/* buffer will be adjusted to span only the bytes not sent */
	auto send(std::span<const std::byte>& buffer, const struct address_v4& target, uint32_t flags) const -> std::size_t;

	auto recv(uint32_t flags) const -> std::tuple<address_v4, std::vector<std::byte>>;
	
	template <typename T>
	requires requires (T const& packet, ::serdes<serdes_writer>& s) 
	{
		{ packet.serdes_size_hint() } -> std::convertible_to<std::size_t>;
		{ packet.serdes(s) } -> std::convertible_to<::serdes<serdes_writer>&>;
	}
	auto send(T const& packet, const address_v4& target, uint32_t flags) const -> std::size_t
	{
		auto buffer_v = serialize_to_vector(packet);
		std::span<const std::byte> buffer_s { buffer_v };
		return send(buffer_s, target, flags);
	}

	template <typename O>
	auto option(const typename O::value_type& value) const -> void
	{
		return socket_option<O>(m_sock, value);
	}

	template <typename O>
	auto option() const -> typename O::value_type
	{
		return socket_option<O>(m_sock);
	}

	template <typename... D>
	void timeout_recv(std::chrono::duration<D...> const& dur)
	{
		using namespace std::chrono;
		const auto to = duration_cast<milliseconds>(dur);
		option<so_rcvtimeo>((std::uint32_t)to.count());		
	}
		
	template <typename... D>
	void timeout_send(std::chrono::duration<D...> const& dur)
	{
		using namespace std::chrono;
		const auto to = duration_cast<milliseconds>(dur);
		option<so_sndtimeo>((std::uint32_t)to.count());		
	}

	template <typename... D>
	void timeout(std::chrono::duration<D...> const& dur)
	{
		using namespace std::chrono;
		const auto to = duration_cast<milliseconds>(dur);
		option<so_rcvtimeo>((std::uint32_t)to.count());
		option<so_sndtimeo>((std::uint32_t)to.count());		
	}

protected:
	socket_udp(int_socket_type int_sock);
private:
	int_socket_type m_sock;
};