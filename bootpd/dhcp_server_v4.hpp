#pragma once

#include <thread>
#include <mutex>

#include <common/config_ini.hpp>
#include <common/lexical_cast.hpp>
#include <common/concurrent_queue.hpp>
#include <common/address_v4.hpp>
#include <common/socket_udp.hpp>

#include "dhcp_options_v4.hpp"
#include "dhcp_packet_v4.hpp"

struct dhcp_server_v4
{
	using packet_queue_type = concurrent_queue<std::tuple<address_v4, std::vector<std::byte>>>;
	
	dhcp_server_v4();
	dhcp_server_v4(config_ini const&);
 ~dhcp_server_v4();
	
	void initialize(config_ini const&);

	void start();
	void cease();
	

protected:
	struct offer_params
	{
		std::uint32_t			client_address;
		std::uint32_t			your_address;
		std::uint32_t			server_address;
		std::uint32_t			gateway_address;
		std::string				boot_file_name;
		std::string				server_host_name;	
		dhcp_options_v4		dhcp_options;			
	};	
	
	using client_map_type = std::unordered_map<std::string, offer_params>;
	
	void initialize_client(offer_params& client_v, config_ini const& cfg, std::string_view client_mac);
	auto make_offer(dhcp_packet_v4 const& packet, offer_params const& client_v) -> dhcp_packet_v4;
	
private:
	void thread_incoming(std::stop_token st);
	void thread_outgoing(std::stop_token st);
	

	socket_udp					m_socket;	
	packet_queue_type		m_packets;
	address_v4					m_bind_address;
	client_map_type     m_clients;
	std::jthread				m_thread_incoming;
	std::jthread				m_thread_outgoing;
};
