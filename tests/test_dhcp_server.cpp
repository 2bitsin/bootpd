#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include <dhcp_server_v4.hpp>

#include "test_helpers.hpp"

using namespace std::chrono_literals;

namespace
{
	constexpr auto test_config = R"(
v4_bind_address = 127.0.0.1
dhcp_listen_port = 0

[00-1C-7E-35-ED-20]
boot_file_name          = pxelinux.0
v4_your_address         = 10.0.0.2
v4_server_address       = 10.0.0.1
v4_subnet_mask          = 255.0.0.0
server_host_name        = bootserver
domain_name             = example.lan
address_lease_time      = 3600

[aa:bb:cc:dd:ee:ff]
v4_your_address         = 10.0.0.3
v4_server_address       = 10.0.0.1
)";

	const std::uint8_t known_mac[] = { 0x00, 0x1c, 0x7e, 0x35, 0xed, 0x20 };
	const std::uint8_t other_mac[] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };
	const std::uint8_t unknown_mac[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

	auto make_request(std::span<const std::uint8_t> mac, std::optional<std::uint8_t> message_type) -> dhcp_packet_v4
	{
		dhcp_packet_v4 request;
		request.opcode(DHCP_OPCODE_REQUEST)
			.hardware_address(mac)
			.transaction_id(0x12345678)
			.flags(DHCP_FLAGS_BROADCAST);
		if (message_type.has_value())
			request.message_type(*message_type);
		return request;
	}

	auto address_option(dhcp_packet_v4 const& packet, std::uint8_t code) -> std::uint32_t
	{
		std::uint32_t value = 0;
		EXPECT_TRUE(packet.options().value(code, std::tie(value))) << "option " << (int)code;
		return value;
	}
}

TEST(dhcp_server, normalizes_mac_addresses)
{
	EXPECT_EQ(dhcp_server_v4::normalize_mac("00:1C:7E:35:ED:20"), "00-1c-7e-35-ed-20");
	EXPECT_EQ(dhcp_server_v4::normalize_mac("00-1c-7e-35-ed-20"), "00-1c-7e-35-ed-20");
}

TEST(dhcp_server, loads_clients_regardless_of_mac_spelling)
{
	// Regression : upper case sections were stored under a key the lookup
	// never used, and lost all their settings.
	const dhcp_server_v4 server(test::config_from(test_config));
	EXPECT_EQ(server.clients().size(), 2u);
	ASSERT_TRUE(server.clients().contains("00-1c-7e-35-ed-20"));
	ASSERT_TRUE(server.clients().contains("aa-bb-cc-dd-ee-ff"));
	EXPECT_EQ(server.clients().at("00-1c-7e-35-ed-20").boot_file_name, "pxelinux.0");
	EXPECT_EQ(server.clients().at("00-1c-7e-35-ed-20").your_address, 0x0a000002u);
}

TEST(dhcp_server, rejects_duplicate_clients)
{
	EXPECT_THROW(dhcp_server_v4(test::config_from("[00-11-22-33-44-55]\nboot_file_name = a\n[00:11:22:33:44:55]\nboot_file_name = b\n")), std::runtime_error);
}

TEST(dhcp_server, rejects_invalid_values)
{
	EXPECT_THROW(dhcp_server_v4(test::config_from("[00-11-22-33-44-55]\naddress_lease_time = forever\n")), std::runtime_error);
	EXPECT_THROW(dhcp_server_v4(test::config_from("dhcp_listen_port = 99999\n")), std::runtime_error);
}

TEST(dhcp_server, offers_on_discover)
{
	const dhcp_server_v4 server(test::config_from(test_config));
	auto request = make_request(known_mac, DHCP_MESSAGE_TYPE_DISCOVER);
	const std::uint8_t requested[] = { DHCP_OPTION_SUBNET_MASK, DHCP_OPTION_BOOT_FILE_NAME, 0, 255 };
	request.options().set(DHCP_OPTION_PARAMETER_REQUEST_LIST, requested);

	const auto reply = server.handle(request);
	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->opcode(), DHCP_OPCODE_RESPONSE);
	EXPECT_EQ(reply->message_type(), DHCP_MESSAGE_TYPE_OFFER);
	EXPECT_EQ(reply->transaction_id(), 0x12345678u);
	EXPECT_EQ(reply->your_address(), 0x0a000002u);
	EXPECT_EQ(reply->server_address(), 0x0a000001u);
	EXPECT_EQ(reply->client_address(), 0u);
	EXPECT_EQ(reply->boot_file_name(), "pxelinux.0");
	EXPECT_EQ(reply->server_host_name(), "bootserver");
	EXPECT_EQ(reply->flags(), DHCP_FLAGS_BROADCAST);
	EXPECT_EQ(address_option(*reply, DHCP_OPTION_SUBNET_MASK), 0xff000000u);
	EXPECT_EQ(address_option(*reply, DHCP_OPTION_SERVER_IDENTIFIER), 0x0a000001u);
	EXPECT_EQ(address_option(*reply, DHCP_OPTION_LEASE_TIME), 3600u);
	const auto boot_file = reply->options()[DHCP_OPTION_BOOT_FILE_NAME];
	EXPECT_EQ(std::string(boot_file.begin(), boot_file.end()), "pxelinux.0");
	const auto domain = reply->options()[DHCP_OPTION_DOMAIN_NAME];
	EXPECT_EQ(std::string(domain.begin(), domain.end()), "example.lan");
}

TEST(dhcp_server, acknowledges_requests)
{
	const dhcp_server_v4 server(test::config_from(test_config));
	auto request = make_request(other_mac, DHCP_MESSAGE_TYPE_REQUEST);
	request.client_address(0x0a000003);
	const auto reply = server.handle(request);
	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->message_type(), DHCP_MESSAGE_TYPE_ACK);
	EXPECT_EQ(reply->your_address(), 0x0a000003u);
	EXPECT_EQ(reply->client_address(), 0x0a000003u);
}

TEST(dhcp_server, ignores_requests_for_another_server)
{
	const dhcp_server_v4 server(test::config_from(test_config));
	auto request = make_request(known_mac, DHCP_MESSAGE_TYPE_REQUEST);
	request.options().set(DHCP_OPTION_SERVER_IDENTIFIER, std::uint32_t(0x0a000063));
	EXPECT_FALSE(server.handle(request).has_value());

	request.options().set(DHCP_OPTION_SERVER_IDENTIFIER, std::uint32_t(0x0a000001));
	EXPECT_TRUE(server.handle(request).has_value());
}

TEST(dhcp_server, answers_plain_bootp)
{
	const dhcp_server_v4 server(test::config_from(test_config));
	const auto reply = server.handle(make_request(known_mac, std::nullopt));
	ASSERT_TRUE(reply.has_value());
	EXPECT_FALSE(reply->message_type().has_value());
	EXPECT_FALSE(reply->options().has(DHCP_OPTION_LEASE_TIME));
	EXPECT_FALSE(reply->options().has(DHCP_OPTION_SERVER_IDENTIFIER));
	EXPECT_EQ(reply->your_address(), 0x0a000002u);
	EXPECT_EQ(reply->boot_file_name(), "pxelinux.0");
}

TEST(dhcp_server, ignores_unknown_clients_and_other_messages)
{
	const dhcp_server_v4 server(test::config_from(test_config));
	EXPECT_FALSE(server.handle(make_request(unknown_mac, DHCP_MESSAGE_TYPE_DISCOVER)).has_value());
	EXPECT_FALSE(server.handle(make_request(known_mac, DHCP_MESSAGE_TYPE_RELEASE)).has_value());
	EXPECT_FALSE(server.handle(make_request(known_mac, DHCP_MESSAGE_TYPE_DECLINE)).has_value());

	auto reply_packet = make_request(known_mac, DHCP_MESSAGE_TYPE_DISCOVER);
	reply_packet.opcode(DHCP_OPCODE_RESPONSE);
	EXPECT_FALSE(server.handle(reply_packet).has_value());

	auto no_mac = make_request({}, DHCP_MESSAGE_TYPE_DISCOVER);
	EXPECT_FALSE(server.handle(no_mac).has_value());
}

TEST(dhcp_server, chooses_the_reply_destination)
{
	const address_v4 source("0.0.0.0:68");
	auto request = make_request(known_mac, DHCP_MESSAGE_TYPE_DISCOVER);
	EXPECT_EQ(dhcp_server_v4::reply_destination(request, source), address_v4("255.255.255.255:68"));

	request.client_address(0x0a000005);
	EXPECT_EQ(dhcp_server_v4::reply_destination(request, source), address_v4("10.0.0.5:68"));

	request.gateway_address(0x0a0000fe);
	EXPECT_EQ(dhcp_server_v4::reply_destination(request, source), address_v4("10.0.0.254:67"));
}

TEST(dhcp_server, serves_over_udp)
{
	dhcp_server_v4 server(test::config_from(test_config));
	server.bind();
	const auto server_address = server.local_address();

	async::io_context context;
	context.spawn(server.run(context));

	std::optional<dhcp_packet_v4> offer;
	bool unknown_answered = false;
	const auto client = [](async::io_context& context, address_v4 server_address,
		std::optional<dhcp_packet_v4>& offer, bool& unknown_answered) -> async::task<void>
	{
		socket_udp socket(address_v4::loopback(0));

		// Unknown clients get no answer at all.
		socket.send(make_request(unknown_mac, DHCP_MESSAGE_TYPE_DISCOVER), server_address);
		unknown_answered = (co_await socket.async_recv(context, 200ms)).has_value();

		// ciaddr set, so the reply is unicast back to us rather than broadcast.
		auto request = make_request(known_mac, DHCP_MESSAGE_TYPE_DISCOVER);
		request.client_address(address_v4::loopback().addr());
		socket.send(request, server_address);
		if (auto reply = co_await socket.async_recv(context, 5s))
			offer.emplace(reply->data);
	};

	ASSERT_TRUE(test::run_until_done(context, client(context, server_address, offer, unknown_answered)));
	EXPECT_FALSE(unknown_answered);
	ASSERT_TRUE(offer.has_value());
	EXPECT_EQ(offer->message_type(), DHCP_MESSAGE_TYPE_OFFER);
	EXPECT_EQ(offer->your_address(), 0x0a000002u);
	EXPECT_EQ(offer->boot_file_name(), "pxelinux.0");
}
