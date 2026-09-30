#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

#include <common/address_v4.hpp>
#include <common/async.hpp>
#include <common/socket_udp.hpp>

#include "tftp_packet.hpp"

// One read (download) transfer, from the RRQ to the final ACK. Each session
// runs as its own coroutine, with its own socket (and thus its own TID).
struct tftp_session_v4
{
	static inline constexpr auto MAX_RETRIES = 5u;

	// Accepted option ranges (RFC 2348, RFC 2349)
	static inline constexpr std::size_t MIN_BLKSIZE = 8u;
	static inline constexpr std::size_t MAX_BLKSIZE = 65464u;
	static inline constexpr std::uint32_t MIN_TIMEOUT = 1u;
	static inline constexpr std::uint32_t MAX_TIMEOUT = 255u;

	struct options_type
	{
		std::size_t   blksize { 512u };
		std::uint32_t timeout { 1u };   // seconds
	};

	struct negotiation_type
	{
		options_type options;
		// Options to acknowledge in an OACK; empty means no OACK is sent.
		tftp_packet::dictionary_type oack;
	};

	// Picks the options to use from those the client requested. Unknown or
	// out of range options are ignored, as RFC 2347 allows.
	static auto negotiate(tftp_packet::dictionary_type const& requested, std::uintmax_t file_size) -> negotiation_type;

	// Maps a requested file name to a path inside `base_dir`. Returns
	// std::nullopt for names that would escape it ("..", absolute paths,
	// symlinks pointing outside). Backslashes are treated as separators and a
	// leading slash is relative to `base_dir`.
	static auto resolve_path(std::filesystem::path const& base_dir, std::string_view filename) -> std::optional<std::filesystem::path>;

	// Serves a read request from `client`, from a new socket bound to `local` (port 0 = any).
	static auto serve_read(async::io_context& context, address_v4 local, std::filesystem::path base_dir,
		address_v4 client, tftp_packet::type_rrq request) -> async::task<void>;

private:
	// Sends `packet` and waits for the ACK of `block_id`, retransmitting on timeout.
	static auto exchange(async::io_context& context, socket_udp const& socket_v, address_v4 client,
		tftp_packet packet, std::uint16_t block_id, std::chrono::seconds timeout) -> async::task<void>;
};
