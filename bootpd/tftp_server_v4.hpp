#pragma once

#include <filesystem>

#include <common/address_v4.hpp>
#include <common/async.hpp>
#include <common/config_ini.hpp>
#include <common/socket_udp.hpp>

#include "tftp_packet.hpp"
#include "tftp_session_v4.hpp"

// Read-only TFTP server. Every accepted read request is handed to a new
// tftp_session_v4 coroutine, so transfers run concurrently.
struct tftp_server_v4
{
	using path = std::filesystem::path;

	explicit tftp_server_v4(config_ini const& cfg);

	// Opens the listening socket. Throws if the root directory doesn't exist
	// or the address can't be bound.
	void bind();

	// Serves requests until the context is stopped.
	auto run(async::io_context& context) -> async::task<void>;

	auto bind_address() const noexcept -> address_v4 const&;
	auto local_address() const -> address_v4;
	auto base_dir() const noexcept -> path const&;

private:
	void dispatch(async::io_context& context, datagram const& datagram_v);

	address_v4 m_address;
	path       m_base_dir;
	socket_udp m_socket;
};
