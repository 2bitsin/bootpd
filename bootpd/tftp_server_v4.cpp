#include <format>
#include <stdexcept>
#include <type_traits>

#include <common/logger.hpp>

#include "tftp_consts.hpp"
#include "tftp_server_v4.hpp"

tftp_server_v4::tftp_server_v4(config_ini const& cfg)
{
	using namespace std::string_view_literals;
	m_address = address_v4(
		cfg.value_or("v4_bind_address"sv, "0.0.0.0"sv),
		cfg.value_or<std::uint16_t>("tftp_listen_port"sv, 69u));
	m_base_dir = cfg.value_or("tftp_base_dir"sv, path("./"));
}

void tftp_server_v4::bind()
{
	std::error_code ec;
	if (!std::filesystem::is_directory(m_base_dir, ec))
		throw std::runtime_error(std::format("TFTP root '{}' is not a directory.", m_base_dir.string()));
	m_base_dir = std::filesystem::canonical(m_base_dir);
	m_socket = socket_udp(m_address);
}

auto tftp_server_v4::bind_address() const noexcept -> address_v4 const&
{
	return m_address;
}

auto tftp_server_v4::local_address() const -> address_v4
{
	return m_socket.local_address();
}

auto tftp_server_v4::base_dir() const noexcept -> path const&
{
	return m_base_dir;
}

auto tftp_server_v4::run(async::io_context& context) -> async::task<void>
{
	Glog.info("TFTP server listening on '{}', serving files from '{}'.", local_address().to_string(), m_base_dir.string());

	for (;;)
	{
		auto datagram_v = co_await m_socket.async_recv(context);
		if (!datagram_v.has_value())
			continue;
		try
		{
			dispatch(context, *datagram_v);
		}
		catch (std::exception const& ex)
		{
			Glog.error("TFTP packet from '{}' : {}", datagram_v->source.to_string(), ex.what());
		}
	}
}

void tftp_server_v4::dispatch(async::io_context& context, datagram const& datagram_v)
{
	const auto& source_v = datagram_v.source;
	const tftp_packet packet_v(datagram_v.data);
	Glog.info("From '{}' received : {}", source_v.to_string(), packet_v.to_string());

	packet_v.visit([&]<typename T>(T const& payload_v)
	{
		if constexpr (std::is_same_v<T, tftp_packet::type_rrq>)
		{
			context.spawn(tftp_session_v4::serve_read(context, m_address, m_base_dir, source_v, payload_v));
		}
		else if constexpr (std::is_same_v<T, tftp_packet::type_wrq>)
		{
			Glog.warning("Refusing upload of '{}' from '{}', uploads are not supported.", payload_v.filename, source_v.to_string());
			m_socket.send(tftp_packet::make_error(tftp_packet::access_violation, "Uploads are not supported."), source_v);
		}
		else if constexpr (std::is_same_v<T, tftp_packet::type_error>)
		{
			// Never answer an error with an error.
		}
		else
		{
			m_socket.send(tftp_packet::make_error(tftp_packet::illegal_operation), source_v);
		}
	});
}
