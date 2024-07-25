#pragma once

#include <common/address_v4.hpp>
#include <common/socket_udp.hpp>
#include <common/config_ini.hpp>
#include <common/concurrent_queue.hpp>

#include <filesystem>
#include <vector>
#include <string>
#include <string_view>
#include <stop_token>
#include <tuple>
#include <unordered_set>

#include "tftp_packet.hpp"
#include "tftp_session_v4.hpp"

struct tftp_server_v4
{
protected:
	
	using event_notify_type = std::tuple<tftp_session_v4 const *>;
	using event_packet_type = std::tuple<address_v4, std::vector<std::byte>>;
		
	using path = std::filesystem::path;
	using event_type = std::variant<event_packet_type, event_notify_type>;
	using event_queue = concurrent_queue<event_type>;
	using session_list = std::unordered_map<tftp_session_v4 const *, std::unique_ptr<tftp_session_v4>>;

public:

	tftp_server_v4();
	tftp_server_v4(config_ini const&);
 ~tftp_server_v4();

	void initialize(config_ini const&);
  void start();
	void cease();

	auto address() const noexcept -> address_v4 const&;
	auto base_dir() const noexcept -> path const&;
	auto session_notify(tftp_session_v4 const* who) -> tftp_server_v4&;

private:
	auto visit_event(event_packet_type const& event_v) -> tftp_server_v4&;
	auto visit_event(event_notify_type const& event_v) -> tftp_server_v4&;
	
	auto visit_packet(tftp_packet::type_rrq const& packet_v, address_v4 const& source_v) -> tftp_server_v4&;
	auto visit_packet(tftp_packet::type_wrq const& packet_v, address_v4 const& source_v) -> tftp_server_v4&;
	auto visit_packet(std::monostate const& packet_v, address_v4 const& source_v) -> tftp_server_v4&;
	
	template <typename T> auto visit_packet(T const& packet_v, address_v4 const& source_v) -> tftp_server_v4&;	
	
	void thread_incoming(std::stop_token st);
	void thread_outgoing(std::stop_token st);
	 
	address_v4		m_address;
	path					m_base_dir;
	socket_udp		m_sock;
	event_queue		m_events;
	session_list	m_session_list;

	std::jthread	m_thread_incoming;
	std::jthread	m_thread_outgoing;
};
