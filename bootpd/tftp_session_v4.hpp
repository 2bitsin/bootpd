#pragma once

#include <stop_token>
#include <thread>
#include <functional>
#include <filesystem>

#include <common/address_v4.hpp>
#include <common/config_ini.hpp>

#include  "tftp_packet.hpp"


struct tftp_server_v4;

struct tftp_session_v4
{
	static inline const constexpr auto MAX_RETRIES = 10u;

	struct options_type
	{
		std::uintmax_t	blksize	{ 512u };
		std::uintmax_t	timeout	{ 1u };
		std::uintmax_t	tsize		{ 0u };
	};

	using notify_func_type = std::function<void(tftp_session_v4 const*)>;

	template <typename P, typename T>
	tftp_session_v4(P& parent, address_v4 source, T const& request)
	:	m_address		{ parent.address().port(0) },
		m_base_dir	{ parent.base_dir() },
		m_thread		{ [&parent, source, request, this] (auto st) { io_thread(parent, source, request, st); } }
	{}

	bool is_done() const;

  void validate_filepath(std::filesystem::path const& file_path_v, socket_udp& socket_v, address_v4 const& remote_client);
	void validate_request(tftp_packet::type_rrq const& request, socket_udp& socket_v, address_v4 const& remote_client);	
	
	void validate_options(options_type& options_v, tftp_packet::type_rrq const& request_v, socket_udp& socket_v, address_v4 const& remote_client_v, std::stop_token token_v);
	void validate_options(options_type& options_v, tftp_packet::type_wrq const& request_v, socket_udp& socket_v, address_v4 const& remote_client_v, std::stop_token token_v);	

  void validate_ack(tftp_packet const& packet_v, socket_udp& socket_v, address_v4 const& remote_client_v, std::uintmax_t number_v);
	void validate_source(address_v4 const& remote_client_v, address_v4 const& from_client_v, socket_udp& socket_v);
	
	void io_thread(tftp_server_v4& parent, address_v4 source, tftp_packet::type_rrq request, std::stop_token st);
	void io_thread(tftp_server_v4& parent, address_v4 source, tftp_packet::type_wrq request, std::stop_token st);

	
private:
	std::atomic<bool> m_done{ false };
	address_v4 m_address;
	std::filesystem::path m_base_dir;
	std::jthread m_thread;
};