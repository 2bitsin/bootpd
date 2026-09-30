#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <common/address_v4.hpp>
#include <common/async.hpp>
#include <common/config_ini.hpp>
#include <common/socket_udp.hpp>

#include "dhcp_options_v4.hpp"
#include "dhcp_packet_v4.hpp"

// Answers DHCP DISCOVER / REQUEST and plain BOOTP requests from the clients
// listed (by MAC address) in the configuration. There is no address pool:
// each known client gets exactly the parameters configured for it.
struct dhcp_server_v4
{
	struct client_config
	{
		std::uint32_t   your_address{ 0 };
		std::uint32_t   server_address{ 0 };
		std::string     boot_file_name;
		std::string     server_host_name;
		dhcp_options_v4 dhcp_options;
	};

	using client_map_type = std::unordered_map<std::string, client_config>;

	explicit dhcp_server_v4(config_ini const& cfg);

	// "00:1C:7E:35:ED:20" -> "00-1c-7e-35-ed-20"
	static auto normalize_mac(std::string_view mac) -> std::string;

	// Opens the listening socket. Throws if the address can't be bound.
	void bind();

	// Serves requests until the context is stopped.
	auto run(async::io_context& context) -> async::task<void>;

	// Builds the reply for a request, std::nullopt if it should be ignored.
	auto handle(dhcp_packet_v4 const& request) const -> std::optional<dhcp_packet_v4>;

	// Where a reply to `request`, received from `source`, must be sent (RFC 2131, 4.1).
	static auto reply_destination(dhcp_packet_v4 const& request, address_v4 const& source) -> address_v4;

	auto bind_address() const noexcept -> address_v4 const&;
	auto local_address() const -> address_v4;
	auto clients() const noexcept -> client_map_type const&;

private:
	static auto load_client(config_ini const& cfg, std::string_view section) -> client_config;
	auto make_reply(dhcp_packet_v4 const& request, client_config const& client) const -> dhcp_packet_v4;

	address_v4      m_bind_address;
	client_map_type m_clients;
	socket_udp      m_socket;
};
