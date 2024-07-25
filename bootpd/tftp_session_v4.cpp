#include "tftp_session_v4.hpp"
#include "tftp_server_v4.hpp"
#include "tftp_reader.hpp"

#include <common/logger.hpp>

#include <fstream>
#include <chrono>
#include <algorithm>

bool tftp_session_v4::is_done() const 
{ return m_done; }

void tftp_session_v4::io_thread(tftp_server_v4& parent_v, address_v4 remote_client_v, tftp_packet::type_rrq request_v, std::stop_token token_v)
{
	using namespace std::chrono_literals;	
	using namespace std::string_view_literals;
	using namespace std::string_literals;
	using std::chrono::system_clock;

	try
	{

		auto socket_v { m_address.make_udp() };
		socket_v.timeout(1s);
		validate_request(request_v, socket_v, remote_client_v);

		auto file_path_v { m_base_dir / request_v.filename };
		validate_filepath(file_path_v, socket_v, remote_client_v);

		options_type options_v
		{
			.blksize	= 512u,
			.timeout	= 1u,
			.tsize		= file_size(file_path_v)
		};
		
		validate_options(options_v, request_v, socket_v, remote_client_v, token_v);		
		socket_v.timeout(1s * options_v.timeout);
		tftp_reader reader_v (file_path_v, options_v.tsize, options_v.blksize, request_v.xfermode == "octet");
		
		Glog.info("Starting transfer of {} to '{}' (file_size = {} bytes, blksize = {} bytes, timeout = {} sec)  ... "sv, 
			request_v.filename, remote_client_v.to_string(), options_v.tsize, options_v.blksize, options_v.timeout);
		
		std::uint32_t retry_counter_v { MAX_RETRIES };		
		for (;!token_v.stop_requested() && retry_counter_v > 0u; --retry_counter_v)
		try
		{			
			socket_v.send(reader_v.data(), remote_client_v, 0);
			auto [from_client_v, packet_bits_v] = socket_v.recv(0);		
			validate_source(remote_client_v, from_client_v, socket_v);
			tftp_packet packet_v(packet_bits_v);
			validate_ack(packet_v, socket_v, remote_client_v, reader_v.number());	
			retry_counter_v = MAX_RETRIES;
			if (reader_v.last())
				break;
			reader_v.next();
		}
		catch(error_socket_timed_out)
		{ continue; }

		if (!retry_counter_v) {
			throw std::runtime_error("Failed to send DATA packet, too many retries."s);
		}

		Glog.info("Finished sending {} to '{}' ... "sv, request_v.filename, remote_client_v.to_string());
	}
	catch (std::exception const& e)
	{ Glog.error("{}"sv, e.what()); }
	catch (...)
	{ Glog.error("Unhandled exception"sv); }
	
	m_done.store(true);
	parent_v.session_notify(this);
}

void tftp_session_v4::validate_source(address_v4 const& remote_client_v, address_v4 const& from_client_v, socket_udp& socket_v)
{
	using namespace std::string_view_literals;
	if (remote_client_v != from_client_v) {
		std::string error = std::format("Expected packet from '{}', instead packet arrived from '{}', ignoring ..."sv, remote_client_v.to_string(), from_client_v.to_string());
		socket_v.send(tftp_packet::make_error(tftp_packet::unknown_transfer_id, "Invalid source address."), from_client_v, 0);
	}
}

void tftp_session_v4::validate_ack(tftp_packet const& packet_v, socket_udp& socket_v, address_v4 const& remote_client_v, std::uintmax_t number_v)
{
	using namespace std::string_view_literals;
	if (packet_v.is<tftp_packet::type_error>()) {
		throw std::runtime_error(std::format("Connection terminated : {}"sv, packet_v.to_string()));
	}

	if (!packet_v.is<tftp_packet::type_ack>()) {
		socket_v.send(tftp_packet::make_error(tftp_packet::illegal_operation), remote_client_v, 0);
		throw std::runtime_error(std::format("Expected ACK packet, received : {}"sv, packet_v.to_string()));
	}

	if (packet_v.as<tftp_packet::type_ack>().block_id != (number_v & 0xffffu)) {
		socket_v.send(tftp_packet::make_error(tftp_packet::illegal_operation), remote_client_v, 0);
		throw std::runtime_error(std::format("Expected ACK to block: {}, received : {}"sv, number_v & 0xffffu, packet_v.as<tftp_packet::type_ack>().block_id));
	}
}

void tftp_session_v4::validate_request(tftp_packet::type_rrq const& request, socket_udp& socket_v, address_v4 const& remote_client)
{
	using namespace std::string_literals;

	if (request.xfermode != "octet"s && request.xfermode != "netascii"s) {
		socket_v.send(tftp_packet::make_error(tftp_packet::illegal_operation), remote_client, 0);
		throw std::runtime_error("Unsupported transfer mode: "s + request.xfermode);
	}
}

void tftp_session_v4::validate_options(options_type& options_v, tftp_packet::type_rrq const& request_v, socket_udp& socket_v, address_v4 const& remote_client_v, std::stop_token token_v)
{
	using namespace std::string_literals;
	using namespace std::chrono_literals;
	tftp_packet::dictionary_type oack_v;

	const auto& dict_v = request_v.options;

	if (dict_v.empty ())
		return;

	if (auto it = dict_v.find("blksize"s); it != dict_v.end()) {
		options_v.blksize = std::stoull((*it).second);
		oack_v.emplace(*it);
	}
	
	if (auto it = dict_v.find("timeout"s); it != dict_v.end()) {
		options_v.timeout = std::stoull((*it).second);
		oack_v.emplace(*it);
	}

	if (auto it = dict_v.find("tsize"s); it != dict_v.end()) {
		oack_v.emplace("tsize"s, std::to_string(options_v.tsize));		
	}
	
	std::uint32_t retry_counter_v { MAX_RETRIES };
	for(;!token_v.stop_requested() && retry_counter_v > 0u; --retry_counter_v)
	try
	{				
		socket_v.send(tftp_packet::make_oack(oack_v), remote_client_v, 0);
		auto [from_client_v, packet_bits_v] = socket_v.recv(0);
		validate_source(remote_client_v, from_client_v, socket_v);
		tftp_packet packet_v(packet_bits_v);
		validate_ack(packet_v, socket_v, remote_client_v, 0u);
		return;
	}
	catch(error_socket_timed_out const&)
	{ continue; }
	
	if (!retry_counter_v) {
		Glog.error("OACK has timed out."s);
		socket_v.send(tftp_packet::make_error(tftp_packet::undefined, "TFTP operation timed out."), remote_client_v, 0);
		throw std::runtime_error("Failed to send OACK packet, too many retries."s);
	}
}

void tftp_session_v4::validate_filepath(std::filesystem::path const& file_path_v, socket_udp& socket_v, address_v4 const& remote_client)
{
	using namespace std::string_literals;
	
	if (!exists(file_path_v)) {
		socket_v.send(tftp_packet::make_error(tftp_packet::file_not_found), remote_client, 0);
		throw std::runtime_error("File not found: "s + file_path_v.string());
	}

	if (!is_regular_file(file_path_v)) {
		socket_v.send(tftp_packet::make_error(tftp_packet::file_not_found), remote_client, 0);
		throw std::runtime_error("Not a file: "s + file_path_v.string());
	}
}

void tftp_session_v4::io_thread(tftp_server_v4& parent_v, address_v4 remote_client_v, tftp_packet::type_wrq request_v, std::stop_token token_v)
{
	using namespace std::chrono_literals;
	auto socket_v = m_address.make_udp();
	
	socket_v.send(tftp_packet::make_error(tftp_packet::access_violation, "Not upload implemented."), remote_client_v, 0);

	Glog.error("TFTP upload not implemented.");
	m_done.store(true);
	parent_v.session_notify(this);
}

