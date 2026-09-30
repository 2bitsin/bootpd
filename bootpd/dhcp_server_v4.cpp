#include <algorithm>
#include <format>

#include <common/logger.hpp>
#include <common/utility_case.hpp>

#include "dhcp_consts_v4.hpp"
#include "dhcp_server_v4.hpp"

namespace
{
	auto message_type_to_string(std::optional<std::uint8_t> message_type) -> std::string
	{
		if (!message_type.has_value())
			return "BOOTREQUEST";
		switch (*message_type)
		{
		case DHCP_MESSAGE_TYPE_DISCOVER: return "DHCPDISCOVER";
		case DHCP_MESSAGE_TYPE_OFFER:    return "DHCPOFFER";
		case DHCP_MESSAGE_TYPE_REQUEST:  return "DHCPREQUEST";
		case DHCP_MESSAGE_TYPE_DECLINE:  return "DHCPDECLINE";
		case DHCP_MESSAGE_TYPE_ACK:      return "DHCPACK";
		case DHCP_MESSAGE_TYPE_NAK:      return "DHCPNAK";
		case DHCP_MESSAGE_TYPE_RELEASE:  return "DHCPRELEASE";
		case DHCP_MESSAGE_TYPE_INFORM:   return "DHCPINFORM";
		default:                         return std::format("DHCP message type {}", *message_type);
		}
	}
}

dhcp_server_v4::dhcp_server_v4(config_ini const& cfg)
{
	using namespace std::string_view_literals;

	m_bind_address = address_v4(
		cfg.value_or("v4_bind_address"sv, "0.0.0.0"sv),
		cfg.value_or<std::uint16_t>("dhcp_listen_port"sv, DHCP_SERVER_PORT));

	for (auto&& section : cfg.sections())
	{
		// The unnamed section holds the global settings.
		if (section.empty())
			continue;
		const auto [it, inserted] = m_clients.emplace(normalize_mac(section), load_client(cfg, section));
		if (!inserted)
			throw std::runtime_error(std::format("Duplicate configuration for client '{}'.", section));
	}
}

auto dhcp_server_v4::normalize_mac(std::string_view mac) -> std::string
{
	auto value_v = lowercase(std::string(mac));
	std::replace(value_v.begin(), value_v.end(), ':', '-');
	return value_v;
}

auto dhcp_server_v4::load_client(config_ini const& cfg, std::string_view section) -> client_config
{
	using namespace std::string_view_literals;

	config_ini::section_type mac(section);
	client_config client_v;

	const auto address_or = [&](std::string_view key, std::string_view fallback) {
		return v4_parse_address(cfg.value_or(mac[key], fallback));
	};

	client_v.your_address = address_or("v4_your_address"sv, "0.0.0.0"sv);
	client_v.server_address = address_or("v4_server_address"sv, "0.0.0.0"sv);
	client_v.server_host_name = cfg.value_or(mac["server_host_name"sv], std::string{});
	client_v.boot_file_name = cfg.value_or(mac["boot_file_name"sv], std::string{});

	if (client_v.server_host_name.size() >= 64u)
		throw std::runtime_error(std::format("[{}] server_host_name must be shorter than 64 characters.", section));
	if (client_v.boot_file_name.size() >= 128u)
		throw std::runtime_error(std::format("[{}] boot_file_name must be shorter than 128 characters.", section));

	const auto server_s = v4_address_to_string(client_v.server_address);
	auto& options_v = client_v.dhcp_options;
	options_v.set(DHCP_OPTION_SUBNET_MASK, address_or("v4_subnet_mask"sv, "0.0.0.0"sv));
	options_v.set(DHCP_OPTION_ROUTER, address_or("v4_router_address"sv, server_s));
	options_v.set(DHCP_OPTION_LOG_SERVER, address_or("v4_log_server_address"sv, server_s));
	options_v.set(DHCP_OPTION_SERVER_IDENTIFIER, address_or("v4_dhcp_server_address"sv, server_s));
	options_v.set(DHCP_OPTION_LEASE_TIME, cfg.value_or(mac["address_lease_time"sv], std::uint32_t(172800)));
	options_v.set(DHCP_OPTION_RENEWAL_TIME, cfg.value_or(mac["address_renewal_time"sv], std::uint32_t(86400)));
	options_v.set(DHCP_OPTION_REBINDING_TIME, cfg.value_or(mac["address_rebinding_time"sv], std::uint32_t(151200)));
	if (!client_v.server_host_name.empty())
		options_v.set(DHCP_OPTION_HOST_NAME, client_v.server_host_name);
	options_v.set(DHCP_OPTION_DOMAIN_NAME, cfg.value_or(mac["domain_name"sv], std::string("localhost")));
	if (!client_v.boot_file_name.empty())
		options_v.set(DHCP_OPTION_BOOT_FILE_NAME, client_v.boot_file_name);

	return client_v;
}

void dhcp_server_v4::bind()
{
	m_socket = socket_udp(m_bind_address);
	m_socket.option<so_broadcast>(so_true);
}

auto dhcp_server_v4::bind_address() const noexcept -> address_v4 const&
{
	return m_bind_address;
}

auto dhcp_server_v4::local_address() const -> address_v4
{
	return m_socket.local_address();
}

auto dhcp_server_v4::clients() const noexcept -> client_map_type const&
{
	return m_clients;
}

auto dhcp_server_v4::run(async::io_context& context) -> async::task<void>
{
	Glog.info("DHCP server listening on '{}', {} client(s) configured.", local_address().to_string(), m_clients.size());

	for (;;)
	{
		auto datagram_v = co_await m_socket.async_recv(context);
		if (!datagram_v.has_value())
			continue;
		try
		{
			Glog.debug("Received {} bytes from '{}'.", datagram_v->data.size(), datagram_v->source.to_string());
			const dhcp_packet_v4 request_v(datagram_v->data);
			if (auto reply_v = handle(request_v); reply_v.has_value())
				m_socket.send(*reply_v, reply_destination(request_v, datagram_v->source));
		}
		catch (std::exception const& ex)
		{
			Glog.error("DHCP packet from '{}' : {}", datagram_v->source.to_string(), ex.what());
		}
	}
}

auto dhcp_server_v4::handle(dhcp_packet_v4 const& request) const -> std::optional<dhcp_packet_v4>
{
	if (request.opcode() != DHCP_OPCODE_REQUEST || request.hardware_address().empty())
		return std::nullopt;

	const auto mac_address_v = normalize_mac(mac_address_to_string(request.hardware_address()));
	const auto message_type_v = request.message_type();
	const auto client_it = m_clients.find(mac_address_v);
	if (client_it == m_clients.end())
	{
		Glog.debug("Ignoring {} from unknown client '{}'.", message_type_to_string(message_type_v), mac_address_v);
		return std::nullopt;
	}
	const auto& client_v = client_it->second;
	auto reply_v = make_reply(request, client_v);

	if (!message_type_v.has_value())
	{
		// Plain BOOTP : no message type, no DHCP lease options.
		for (auto code : { DHCP_OPTION_SERVER_IDENTIFIER, DHCP_OPTION_LEASE_TIME, DHCP_OPTION_RENEWAL_TIME, DHCP_OPTION_REBINDING_TIME })
			reply_v.options().erase(code);
		reply_v.assign_options(client_v.dhcp_options, { DHCP_OPTION_SUBNET_MASK, DHCP_OPTION_ROUTER, DHCP_OPTION_HOST_NAME, DHCP_OPTION_DOMAIN_NAME });
		Glog.info("Responding to BOOTREQUEST from '{}' (xid {:#010x}) with BOOTREPLY.", mac_address_v, request.transaction_id());
		return reply_v;
	}

	switch (*message_type_v)
	{
	case DHCP_MESSAGE_TYPE_DISCOVER:
		reply_v.message_type(DHCP_MESSAGE_TYPE_OFFER);
		reply_v.client_address(0u);
		break;

	case DHCP_MESSAGE_TYPE_REQUEST:
		// A client that selected another server's offer names that server here.
		if (request.options().has(DHCP_OPTION_SERVER_IDENTIFIER)
			&& !std::ranges::equal(request.options()[DHCP_OPTION_SERVER_IDENTIFIER], client_v.dhcp_options[DHCP_OPTION_SERVER_IDENTIFIER]))
		{
			Glog.debug("Ignoring DHCPREQUEST from '{}' addressed to another server.", mac_address_v);
			return std::nullopt;
		}
		reply_v.message_type(DHCP_MESSAGE_TYPE_ACK);
		break;

	default:
		Glog.debug("Ignoring {} from '{}'.", message_type_to_string(message_type_v), mac_address_v);
		return std::nullopt;
	}

	Glog.info("Responding to {} from '{}' (xid {:#010x}) with {}, offering {}.",
		message_type_to_string(message_type_v), mac_address_v, request.transaction_id(),
		message_type_to_string(reply_v.message_type()), v4_address_to_string(reply_v.your_address()));
	return reply_v;
}

auto dhcp_server_v4::make_reply(dhcp_packet_v4 const& request, client_config const& client) const -> dhcp_packet_v4
{
	return (dhcp_packet_v4()
		.opcode(DHCP_OPCODE_RESPONSE)
		.hardware_type(request.hardware_type())
		.hardware_address(request.hardware_address())
		.number_of_hops(0)
		.flags(request.flags())
		.seconds_elapsed(0)
		.transaction_id(request.transaction_id())
		.client_address(request.client_address())
		.your_address(client.your_address)
		.server_address(client.server_address)
		.gateway_address(request.gateway_address())
		.boot_file_name(client.boot_file_name)
		.server_host_name(client.server_host_name)
		.assign_options(client.dhcp_options, request.requested_parameters())
		.assign_options(client.dhcp_options, {
			DHCP_OPTION_SUBNET_MASK, DHCP_OPTION_SERVER_IDENTIFIER, DHCP_OPTION_LEASE_TIME,
			DHCP_OPTION_RENEWAL_TIME, DHCP_OPTION_REBINDING_TIME, DHCP_OPTION_LOG_SERVER,
			DHCP_OPTION_DOMAIN_NAME })
	);
}

auto dhcp_server_v4::reply_destination(dhcp_packet_v4 const& request, address_v4 const& source) -> address_v4
{
	// Through a relay agent : back to the relay, on the server port.
	if (request.gateway_address() != 0u)
		return address_v4(request.gateway_address(), DHCP_SERVER_PORT);
	// The client already has an address it can receive unicast on.
	if (request.client_address() != 0u)
		return address_v4(request.client_address(), source.port());
	// Otherwise the client can't receive unicast yet.
	return address_v4::everyone(source.port());
}
