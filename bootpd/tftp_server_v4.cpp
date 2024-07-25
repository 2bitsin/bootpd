#include <chrono>
#include <functional>
#include <filesystem>

#include <common/logger.hpp>
#include <common/address_v4.hpp>
#include <common/socket_udp.hpp>
#include <common/socket_error.hpp>

#include "tftp_server_v4.hpp"
#include "tftp_packet.hpp"
#include "tftp_consts.hpp"


tftp_server_v4::tftp_server_v4()
{
}

tftp_server_v4::tftp_server_v4(config_ini const& cfg)
{
	initialize(cfg);
}

tftp_server_v4::~tftp_server_v4()
{
	cease();
}

void tftp_server_v4::initialize(config_ini const& cfg)
{
	using namespace std::string_view_literals;
	m_address = cfg.value_or("v4_bind_address"sv, address_v4::any()).port(cfg.value_or("tftp_listen_port"sv, 69));
	m_base_dir = cfg.value_or("tftp_base_dir"sv, std::filesystem::path("./"));	
}

void tftp_server_v4::start()
{
	using namespace std::chrono_literals;
	Glog.info("Starting TFTP server on '{}', with root at '{}' ... ", m_address.to_string(), std::filesystem::absolute(m_base_dir).string());
	m_sock = m_address.make_udp();
	m_sock.timeout(500ms);	
	m_thread_incoming = std::jthread([this](auto&& st){ thread_incoming (st); });
	m_thread_outgoing = std::jthread([this](auto&& st){ thread_outgoing (st); });
	std::this_thread::sleep_for(10ms);
}

void tftp_server_v4::cease()
{
	Glog.info("Stopping TFTP server on '{}' ... ", m_address.to_string());
	if (m_thread_incoming.joinable()) {
		m_thread_incoming.request_stop();
		m_thread_incoming.join();
	}
	if (m_thread_outgoing.joinable()) {
		m_thread_outgoing.request_stop();
		m_thread_outgoing.join();
	}
}

auto tftp_server_v4::address() const noexcept -> address_v4 const&
{ return m_address; }

auto tftp_server_v4::base_dir() const noexcept -> path const&
{ return m_base_dir; }

void tftp_server_v4::thread_incoming(std::stop_token st)
{
	using namespace std::string_view_literals;
	Glog.info("* Receiver thread started.");
	while (!st.stop_requested()) 
	{
		try
		{
			auto[source, packet_bits] = m_sock.recv(0);
			if (packet_bits.empty()) 
				continue;
			Glog.info("Received {} byte TFTP packet from '{}' ... ", packet_bits.size(), source.to_string());
			m_events.emplace(event_packet_type(std::move(source), std::move(packet_bits)));
		}
		catch (error_socket_timed_out const&)
		{ continue; }
		catch (std::exception const& e)
		{ Glog.error("{}"sv, e.what()); }
	}
	Glog.info("* Receiver thread stopped.");
}

auto tftp_server_v4::session_notify(tftp_session_v4 const* who) -> tftp_server_v4&
{
	m_events.push(event_notify_type(who));
	return *this;
}

template<typename T>
auto tftp_server_v4::visit_packet(T const& packet_v, address_v4 const& source_v) -> tftp_server_v4&
{
	m_sock.send(tftp_packet::make_error(tftp_packet::illegal_operation), source_v, 0);
	Glog.info("Ignoring non-request packet from '{}'.", source_v.to_string());
	return *this;
}

auto tftp_server_v4::visit_event(event_packet_type const& event_v) -> tftp_server_v4&
{
	auto [source_v, packet_bits] = event_v;
	tftp_packet packet_v (packet_bits);			
	Glog.info("From '{}' received : {} ", source_v.to_string(), packet_v.to_string());
	packet_v.visit([this, source_v](auto&& packet_v){ 
		visit_packet(packet_v, source_v); 
	});
	return *this;
}

auto tftp_server_v4::visit_event(event_notify_type const& event_v) -> tftp_server_v4&
{
	auto [session_ptr] = event_v;
	if (session_ptr && (*session_ptr).is_done()) {
		Glog.debug("Killing session {:#08x} ...", (std::uintptr_t)session_ptr);
		m_session_list.erase (session_ptr);			
	}
	return *this;
}

auto tftp_server_v4::visit_packet(tftp_packet::type_rrq const& packet_v, address_v4 const& source_v) -> tftp_server_v4&
{
	auto session_ptr = std::make_unique<tftp_session_v4>(*this, source_v, packet_v);
	m_session_list.emplace(session_ptr.get(), std::move(session_ptr));
	return *this;
}

auto tftp_server_v4::visit_packet(tftp_packet::type_wrq const& packet_v, address_v4 const& source_v) -> tftp_server_v4&
{
	auto session_ptr = std::make_unique<tftp_session_v4>(*this, source_v, packet_v);
	m_session_list.emplace(session_ptr.get(), std::move(session_ptr));
	return *this;
}

auto tftp_server_v4::visit_packet(std::monostate const& packet_v, address_v4 const& source_v) -> tftp_server_v4&
{
	throw std::logic_error("Empty packet, packet parsing failed.");
	return *this;
}

void tftp_server_v4::thread_outgoing(std::stop_token st)
{
	Glog.info("* Responder thread started.");
	using namespace std::string_view_literals;

	while (!st.stop_requested()) 
	{
		try
		{											
			std::visit([this](auto&& event_v) { 
				visit_event(event_v); 
			}, m_events.pop(st));							
		}
		catch (error_socket_timed_out const&)
		{ continue; }
		catch (error_stop_requested const&)
		{ break; }
		catch (std::exception const& e)
		{ Glog.error("{}"sv, e.what()); }		
	}
	Glog.info("* Responder thread stopped.");
}


