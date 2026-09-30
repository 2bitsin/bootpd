#include "tftp_session_v4.hpp"
#include "tftp_reader.hpp"

#include <common/lexical_cast.hpp>
#include <common/logger.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <stdexcept>
#include <string>
#include <system_error>

namespace
{
	template <typename T>
	auto parse_option(tftp_packet::dictionary_type const& options, std::string const& name, T min_value, T max_value) -> std::optional<T>
	{
		const auto it = options.find(name);
		if (it == options.end())
			return std::nullopt;
		try
		{
			const auto value_v = lexical_cast<T>(it->second);
			if (value_v < min_value || value_v > max_value)
				return std::nullopt;
			return value_v;
		}
		catch (std::exception const&)
		{
			return std::nullopt;
		}
	}

	// Removes a trailing empty element ("dir/" -> "dir").
	auto without_trailing_separator(std::filesystem::path path) -> std::filesystem::path
	{
		if (!path.empty() && !path.has_filename() && path.has_parent_path() && path != path.root_path())
			path = path.parent_path();
		return path;
	}
}

auto tftp_session_v4::negotiate(tftp_packet::dictionary_type const& requested, std::uintmax_t file_size) -> negotiation_type
{
	negotiation_type result_v;

	if (auto blksize = parse_option<std::size_t>(requested, "blksize", MIN_BLKSIZE, MAX_BLKSIZE)) {
		result_v.options.blksize = *blksize;
		result_v.oack.emplace("blksize", std::to_string(*blksize));
	}

	if (auto timeout = parse_option<std::uint32_t>(requested, "timeout", MIN_TIMEOUT, MAX_TIMEOUT)) {
		result_v.options.timeout = *timeout;
		result_v.oack.emplace("timeout", std::to_string(*timeout));
	}

	// In a read request the client sends "0", the server answers with the size.
	if (requested.contains("tsize"))
		result_v.oack.emplace("tsize", std::to_string(file_size));

	return result_v;
}

auto tftp_session_v4::resolve_path(std::filesystem::path const& base_dir, std::string_view filename) -> std::optional<std::filesystem::path>
{
	namespace fs = std::filesystem;

	std::string name_v{ filename };
	if (name_v.empty() || name_v.find('\0') != std::string::npos)
		return std::nullopt;
	std::replace(name_v.begin(), name_v.end(), '\\', '/');
	name_v.erase(0, name_v.find_first_not_of('/'));

	const auto relative_v = fs::path(name_v).lexically_normal();
	if (relative_v.empty() || relative_v.has_root_path() || relative_v.has_root_name())
		return std::nullopt;
	if (auto first = relative_v.begin(); first != relative_v.end() && *first == "..")
		return std::nullopt;

	std::error_code ec;
	const auto base_v = without_trailing_separator(fs::weakly_canonical(base_dir, ec));
	if (ec)
		return std::nullopt;
	const auto full_v = without_trailing_separator(fs::weakly_canonical(base_v / relative_v, ec));
	if (ec)
		return std::nullopt;

	// Resolving symlinks may still have led outside of the base directory.
	const auto [base_end, full_it] = std::mismatch(base_v.begin(), base_v.end(), full_v.begin(), full_v.end());
	if (base_end != base_v.end() || full_it == full_v.end())
		return std::nullopt;

	return full_v;
}

auto tftp_session_v4::exchange(async::io_context& context, socket_udp const& socket_v, address_v4 client,
	tftp_packet packet, std::uint16_t block_id, std::chrono::seconds timeout) -> async::task<void>
{
	const auto bits_v = serialize_to_vector(packet);

	for (auto attempt = 0u; attempt < MAX_RETRIES; ++attempt)
	{
		if (attempt > 0u)
			Glog.debug("No ACK for block {} from '{}', retransmitting ({}/{}).", block_id, client.to_string(), attempt, MAX_RETRIES - 1u);

		socket_v.send(std::span<const std::byte>{ bits_v }, client);
		const auto deadline = async::io_context::clock::now() + timeout;

		for (;;)
		{
			auto datagram_v = co_await socket_v.async_recv(context, deadline);
			if (!datagram_v.has_value())
				break;

			// A packet from someone else must not disturb this transfer (RFC 1350, 4).
			if (datagram_v->source != client)
			{
				Glog.warning("Unexpected packet from '{}' during transfer to '{}'.", datagram_v->source.to_string(), client.to_string());
				socket_v.send(tftp_packet::make_error(tftp_packet::unknown_transfer_id), datagram_v->source);
				continue;
			}

			std::optional<tftp_packet> reply_v;
			try { reply_v.emplace(datagram_v->data); }
			catch (std::exception const&) {}

			if (!reply_v.has_value())
			{
				socket_v.send(tftp_packet::make_error(tftp_packet::illegal_operation, "Malformed packet."), client);
				throw std::runtime_error("received a malformed packet.");
			}

			if (reply_v->is<tftp_packet::type_error>())
				throw std::runtime_error(std::format("aborted by client : {}", reply_v->to_string()));

			if (!reply_v->is<tftp_packet::type_ack>())
			{
				socket_v.send(tftp_packet::make_error(tftp_packet::illegal_operation), client);
				throw std::runtime_error(std::format("expected ACK, received : {}", reply_v->to_string()));
			}

			if (reply_v->as<tftp_packet::type_ack>().block_id == block_id)
				co_return;

			// A duplicate ACK of an earlier block. Don't retransmit in response,
			// that would double the traffic (the "Sorcerer's Apprentice" bug).
			Glog.trace("Ignoring duplicate ACK {} from '{}'.", reply_v->as<tftp_packet::type_ack>().block_id, client.to_string());
		}
	}

	socket_v.send(tftp_packet::make_error(tftp_packet::undefined, "Timed out."), client);
	throw std::runtime_error(std::format("timed out waiting for ACK of block {}.", block_id));
}

auto tftp_session_v4::serve_read(async::io_context& context, address_v4 local, std::filesystem::path base_dir,
	address_v4 client, tftp_packet::type_rrq request) -> async::task<void>
{
	using clock = std::chrono::steady_clock;

	socket_udp socket_v(local.port(0));
	const auto client_s = client.to_string();

	const auto reject = [&](tftp_packet::error_category_type code, std::string_view reason) {
		Glog.warning("Refusing to send '{}' to '{}' : {}", request.filename, client_s, reason);
		socket_v.send(tftp_packet::make_error(code), client);
	};

	// "netascii" is served verbatim, like most TFTP servers used for network booting do.
	if (request.xfermode != "octet" && request.xfermode != "netascii") {
		reject(tftp_packet::illegal_operation, std::format("unsupported transfer mode '{}'", request.xfermode));
		co_return;
	}

	const auto path_v = resolve_path(base_dir, request.filename);
	if (!path_v.has_value()) {
		reject(tftp_packet::access_violation, "path is outside of the TFTP root");
		co_return;
	}

	std::error_code ec;
	if (!std::filesystem::is_regular_file(*path_v, ec)) {
		reject(tftp_packet::file_not_found, std::format("'{}' does not exist or is not a file", path_v->string()));
		co_return;
	}

	const auto file_size_v = std::filesystem::file_size(*path_v, ec);
	if (ec) {
		reject(tftp_packet::access_violation, ec.message());
		co_return;
	}

	const auto negotiation_v = negotiate(request.options, file_size_v);
	const auto& options_v = negotiation_v.options;

	std::optional<tftp_reader> reader_v;
	try { reader_v.emplace(*path_v, options_v.blksize); }
	catch (std::exception const& ex) {
		reject(tftp_packet::access_violation, ex.what());
		co_return;
	}

	Glog.info("Sending '{}' to '{}' ({} bytes, blksize = {}, timeout = {}s) ...",
		request.filename, client_s, file_size_v, options_v.blksize, options_v.timeout);

	const auto started_v = clock::now();
	const auto timeout_v = std::chrono::seconds(options_v.timeout);
	try
	{
		if (!negotiation_v.oack.empty())
			co_await exchange(context, socket_v, client, tftp_packet::make_oack(negotiation_v.oack), 0u, timeout_v);

		while (reader_v->next())
			co_await exchange(context, socket_v, client, reader_v->data(), reader_v->block_id(), timeout_v);
	}
	catch (std::exception const& ex)
	{
		Glog.error("Transfer of '{}' to '{}' failed : {}", request.filename, client_s, ex.what());
		co_return;
	}

	const auto elapsed_v = std::chrono::duration<double>(clock::now() - started_v).count();
	Glog.info("Finished sending '{}' to '{}' in {:.2f}s.", request.filename, client_s, elapsed_v);
}
