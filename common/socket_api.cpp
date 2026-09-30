#include <algorithm>
#include <cstring>
#include <format>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <mswsock.h>
	#include <mstcpip.h>
	#include <windows.h>
	#ifdef _MSC_VER
		#pragma comment(lib, "ws2_32.lib")
	#endif
	#ifndef SIO_UDP_CONNRESET
		#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
	#endif
#else
	#include <arpa/inet.h>
	#include <cerrno>
	#include <fcntl.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <poll.h>
	#include <sys/socket.h>
	#include <unistd.h>
#endif

#include "byte_order.hpp"
#include "socket_api.hpp"
#include "address_v4.hpp"

namespace
{
#ifdef _WIN32
	using native_socket = SOCKET;
	using socklen_type = int;
	constexpr int error_would_block = WSAEWOULDBLOCK;
	constexpr int error_interrupted = WSAEINTR;
	constexpr int error_conn_reset = WSAECONNRESET;
	constexpr int error_msg_size = WSAEMSGSIZE;

	auto socket_last_error() -> int { return WSAGetLastError(); }
#else
	using native_socket = int;
	using socklen_type = socklen_t;
	constexpr int error_would_block = EWOULDBLOCK;
	constexpr int error_interrupted = EINTR;
	constexpr int error_conn_reset = ECONNREFUSED;
	constexpr int error_msg_size = EMSGSIZE;

	auto socket_last_error() -> int { return errno; }
#endif

	auto is_would_block(int error) -> bool
	{
#ifndef _WIN32
		if (error == EAGAIN)
			return true;
#endif
		return error == error_would_block;
	}

	auto last_error_as_string(int error = socket_last_error()) -> std::string
	{
		return std::format("{} ({})", std::system_category().message(error), error);
	}

	auto native(int_socket_type socket) -> native_socket
	{
		return static_cast<native_socket>(socket);
	}

	void v4_initialize()
	{
#ifdef _WIN32
		static std::once_flag once;
		std::call_once(once, []()
		{
			WSADATA wsad{};
			if (auto wsaerr = WSAStartup(MAKEWORD(2, 2), &wsad); wsaerr != 0)
				throw std::runtime_error("WSAStartup failed : " + last_error_as_string(wsaerr));
			std::atexit([]() { WSACleanup(); });
		});
#endif
	}

	auto to_sockaddr(address_v4 const& address) -> sockaddr_in
	{
		sockaddr_in target;
		std::memset(&target, 0, sizeof(target));
		target.sin_family = AF_INET;
		target.sin_port = address.net_port();
		target.sin_addr.s_addr = address.net_addr();
		return target;
	}

	auto from_sockaddr(sockaddr_in const& what) -> address_v4
	{
		if (what.sin_family != AF_INET)
			throw std::logic_error("address family mismatch.");
		return address_v4(net_to_host<std::uint32_t>(what.sin_addr.s_addr), net_to_host<std::uint16_t>(what.sin_port));
	}
}

auto v4_resolve_single(std::string_view target) -> std::uint32_t
{
	v4_initialize();

	std::string host_v{ target };
	addrinfo hints{};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo* result = nullptr;
	if (const auto error = getaddrinfo(host_v.c_str(), nullptr, &hints, &result); error != 0 || result == nullptr)
		throw std::runtime_error(std::format("'{}' cannot be resolved to an IPv4 address.", host_v));
	const auto address = reinterpret_cast<const sockaddr_in*>(result->ai_addr)->sin_addr.s_addr;
	freeaddrinfo(result);
	return net_to_host<std::uint32_t>(address);
}

auto v4_socket_make_invalid() -> int_socket_type
{
#ifdef _WIN32
	return static_cast<int_socket_type>(INVALID_SOCKET);
#else
	return -1;
#endif
}

auto v4_socket_make_udp() -> int_socket_type
{
	v4_initialize();

	const auto int_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (static_cast<int_socket_type>(int_sock) == v4_socket_make_invalid())
		throw std::runtime_error("can't create socket : " + last_error_as_string());

#ifdef _WIN32
	// Without this, an ICMP "port unreachable" caused by an earlier sendto()
	// makes the next recvfrom() fail with WSAECONNRESET.
	BOOL new_behavior = FALSE;
	DWORD bytes_returned = 0;
	WSAIoctl(int_sock, SIO_UDP_CONNRESET, &new_behavior, sizeof(new_behavior), nullptr, 0, &bytes_returned, nullptr, nullptr);
#endif

	return static_cast<int_socket_type>(int_sock);
}

auto v4_socket_make_udp(const address_v4& address) -> int_socket_type
{
	const auto int_sock = v4_socket_make_udp();
	try
	{
		v4_socket_set_nonblocking(int_sock);
		v4_socket_bind(int_sock, address);
	}
	catch (...)
	{
		v4_socket_close(int_sock);
		throw;
	}
	return int_sock;
}

void v4_socket_set_nonblocking(int_socket_type socket)
{
#ifdef _WIN32
	u_long mode = 1;
	if (ioctlsocket(native(socket), FIONBIO, &mode) != 0)
		throw std::runtime_error("failed to make socket non-blocking : " + last_error_as_string());
#else
	const auto flags = fcntl(native(socket), F_GETFL, 0);
	if (flags < 0 || fcntl(native(socket), F_SETFL, flags | O_NONBLOCK) < 0)
		throw std::runtime_error("failed to make socket non-blocking : " + last_error_as_string());
#endif
}

void v4_socket_bind(int_socket_type socket, const address_v4& address)
{
	v4_initialize();

	const auto sai = to_sockaddr(address);
	if (const auto error = bind(native(socket), (const sockaddr*)&sai, sizeof(sai)); error != 0)
		throw std::runtime_error(std::format("failed to bind socket to '{}' : {}", address.to_string(), last_error_as_string()));
}

auto v4_socket_local_address(int_socket_type socket) -> address_v4
{
	sockaddr_in sai{};
	socklen_type len = sizeof(sai);
	if (getsockname(native(socket), (sockaddr*)&sai, &len) != 0)
		throw std::runtime_error("getsockname failed : " + last_error_as_string());
	return from_sockaddr(sai);
}

auto v4_parse_address_and_port(std::string_view what) -> std::pair<std::uint32_t, std::uint16_t>
{
	v4_initialize();

	std::uint16_t port{ 0 };
	if (auto it = what.find(':'); it != what.npos) {
		const std::string port_s{ what.substr(it + 1) };
		if (!port_s.empty())
		{
			std::size_t idx = 0;
			unsigned long value = 0;
			try { value = std::stoul(port_s, &idx, 10); }
			catch (std::exception const&) { idx = 0; }
			if (idx != port_s.size() || value > 65535u)
				throw std::invalid_argument(port_s + " is not a valid port number.");
			port = static_cast<std::uint16_t>(value);
		}
		what = what.substr(0, it);
	}

	const std::string address_s{ what };
	in_addr address{};
	if (inet_pton(AF_INET, address_s.c_str(), &address) != 1)
		return { v4_resolve_single(address_s), port };
	return { net_to_host<std::uint32_t>(address.s_addr), port };
}

auto v4_address_to_string(std::uint32_t address) -> std::string
{
	return std::format("{}.{}.{}.{}", (address >> 24) & 0xffu, (address >> 16) & 0xffu, (address >> 8) & 0xffu, address & 0xffu);
}

auto v4_parse_address(std::string_view what) -> std::uint32_t
{
	return v4_parse_address_and_port(what).first;
}

void v4_socket_close(int_socket_type socket)
{
#ifdef _WIN32
	closesocket(native(socket));
#else
	close(native(socket));
#endif
}

auto v4_socket_recv(int_socket_type socket, std::span<std::byte>& buffer, address_v4& address) -> bool
{
	for (;;)
	{
		sockaddr_in addr_in{};
		socklen_type addr_len = sizeof(addr_in);
		const auto size = (int)std::min<std::size_t>(buffer.size(), 0x7fffffffu);

		const auto received_bytes = recvfrom(native(socket), (char*)buffer.data(), size, 0, (sockaddr*)&addr_in, &addr_len);
		if (received_bytes >= 0)
		{
			address = from_sockaddr(addr_in);
			buffer = buffer.subspan(0, (std::size_t)received_bytes);
			return true;
		}

		const auto error_code = socket_last_error();
		if (is_would_block(error_code))
			return false;
		// Interrupted calls, ICMP errors from earlier sends and oversized
		// datagrams only affect a single packet, carry on with the next one.
		if (error_code == error_interrupted || error_code == error_conn_reset || error_code == error_msg_size)
			continue;

		throw std::runtime_error("failed to receive from socket : " + last_error_as_string(error_code));
	}
}

auto v4_socket_send(int_socket_type socket, std::span<const std::byte> buffer, const address_v4& address) -> std::size_t
{
	const auto addr_in = to_sockaddr(address);
	const auto size = (int)std::min<std::size_t>(buffer.size(), 0x7fffffffu);

	for (;;)
	{
		const auto sent_bytes = sendto(native(socket), (const char*)buffer.data(), size, 0, (const sockaddr*)&addr_in, sizeof(addr_in));
		if (sent_bytes >= 0)
			return (std::size_t)sent_bytes;

		const auto error_code = socket_last_error();
		if (error_code == error_interrupted)
			continue;
		// UDP is lossy anyway, a full send buffer is equivalent to a dropped packet.
		if (is_would_block(error_code))
			return 0u;
		throw std::runtime_error(std::format("failed to send to '{}' : {}", address.to_string(), last_error_as_string(error_code)));
	}
}

void v4_socket_poll(std::span<socket_poll_entry> entries, int timeout_ms)
{
	std::vector<pollfd> fds(entries.size());
	for (std::size_t i = 0; i < entries.size(); ++i)
	{
		fds[i].fd = native(entries[i].socket);
		fds[i].events = POLLIN;
		fds[i].revents = 0;
		entries[i].readable = false;
	}

#ifdef _WIN32
	if (fds.empty()) {
		Sleep((DWORD)timeout_ms);
		return;
	}
	const auto result = WSAPoll(fds.data(), (ULONG)fds.size(), timeout_ms);
#else
	const auto result = poll(fds.data(), (nfds_t)fds.size(), timeout_ms);
#endif

	if (result < 0)
	{
		const auto error_code = socket_last_error();
		if (error_code == error_interrupted)
			return;
		throw std::runtime_error("poll failed : " + last_error_as_string(error_code));
	}

	for (std::size_t i = 0; i < entries.size(); ++i)
	{
		// Errors are reported as readable, the following recv surfaces them.
		entries[i].readable = (fds[i].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL)) != 0;
	}
}

static auto to_hex(std::uint8_t value) -> std::string
{
	static constexpr const char x [] = "0123456789ABCDEF";
	return std::string{ x[(value >> 4) & 0xf], x[value & 0xf] };
}

auto mac_address_to_string(std::span<const std::uint8_t> data)
	-> std::string
{
	if (data.empty())
		throw std::runtime_error("hardware address is empty");

	std::string value;
	value.append(to_hex(data.front()));
	for (auto&& a_byte : data.subspan(1u))
	{
		value.push_back('-');
		value.append(to_hex(a_byte));
	}
	return value;
}

namespace detail
{
	void socket_option_set(int_socket_type target, int level, int option, const void* value, int size)
	{
		if (setsockopt(native(target), level, option, (const char*)value, (socklen_type)size) != 0)
			throw std::runtime_error("failed to set socket option : " + last_error_as_string());
	}

	void socket_option_get(int_socket_type target, int level, int option, void* value, int size)
	{
		auto actual_size = (socklen_type)size;
		if (getsockopt(native(target), level, option, (char*)value, &actual_size) != 0)
			throw std::runtime_error("failed to get socket option : " + last_error_as_string());
		if (actual_size != (socklen_type)size)
			throw std::logic_error("socket option value size mismatch.");
	}
}
