#include <chrono>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <common/lexical_cast.hpp>
#include <tftp_server_v4.hpp>

#include "test_helpers.hpp"

using namespace std::chrono_literals;

namespace
{
	struct download_result
	{
		std::vector<std::byte> data;
		std::optional<tftp_packet::type_error> error;
		std::optional<tftp_packet::dictionary_type> oack;
		std::size_t data_packets{ 0 };
		std::size_t retransmissions{ 0 };
	};

	struct client_behaviour
	{
		bool ignore_first_data{ false };  // don't ACK the first DATA, forcing a retransmission
		bool duplicate_acks{ false };     // send every ACK twice
	};

	// A minimal TFTP client, good enough to exercise the server.
	auto download(async::io_context& context, address_v4 server, tftp_packet request, client_behaviour behaviour = {})
		-> async::task<download_result>
	{
		socket_udp socket(address_v4::loopback(0));
		socket.send(request, server);

		download_result result;
		std::size_t blksize = 512;
		std::uint16_t expected = 1;
		bool ignored = false;

		for (;;)
		{
			auto datagram_v = co_await socket.async_recv(context, 10s);
			if (!datagram_v)
				throw std::runtime_error("client timed out");
			const tftp_packet packet(datagram_v->data);
			const auto peer = datagram_v->source;

			if (packet.is<tftp_packet::type_error>()) {
				result.error = packet.as<tftp_packet::type_error>();
				co_return result;
			}
			if (packet.is<tftp_packet::type_oack>()) {
				result.oack = packet.as<tftp_packet::type_oack>().options;
				if (auto it = result.oack->find("blksize"); it != result.oack->end())
					blksize = lexical_cast<std::size_t>(it->second);
				socket.send(tftp_packet::make_ack(0), peer);
				continue;
			}
			if (!packet.is<tftp_packet::type_data>())
				throw std::runtime_error("unexpected packet " + packet.to_string());

			const auto& data = packet.as<tftp_packet::type_data>();
			++result.data_packets;
			if (data.block_id != expected) {
				++result.retransmissions;
				continue;
			}
			if (behaviour.ignore_first_data && !ignored) {
				ignored = true;
				continue;
			}
			result.data.insert(result.data.end(), data.data.begin(), data.data.end());
			socket.send(tftp_packet::make_ack(data.block_id), peer);
			if (behaviour.duplicate_acks)
				socket.send(tftp_packet::make_ack(data.block_id), peer);
			++expected;
			if (data.data.size() < blksize)
				co_return result;
		}
	}

	class tftp_server_test: public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			root = dir.path() / "root";
			dir.write("root/small.bin", test::pattern(1300));
			dir.write("root/exact.bin", test::pattern(2048));
			dir.write("root/sub/dir/file.txt", test::bytes("hello"));
			dir.write("secret.txt", test::bytes("secret"));

			server.emplace(test::config_from(std::format(
				"v4_bind_address = 127.0.0.1\ntftp_listen_port = 0\ntftp_base_dir = \"{}\"\n", root.generic_string())));
			server->bind();
			context.spawn(server->run(context));
		}

		// Runs the downloads concurrently against the server. The context (and
		// the server) stop afterwards, so call this only once per test.
		auto run(std::vector<tftp_packet> requests, client_behaviour behaviour = {}) -> std::vector<download_result>
		{
			std::vector<download_result> results(requests.size());
			const auto all = [](async::io_context& context, address_v4 server, std::vector<tftp_packet> requests,
				client_behaviour behaviour, std::vector<download_result>& results) -> async::task<void>
			{
				// Start all downloads, then wait for each of them.
				std::vector<async::task<download_result>> tasks;
				for (auto& request : requests)
					tasks.push_back(download(context, server, request, behaviour));
				for (std::size_t i = 0; i < tasks.size(); ++i)
					results[i] = co_await std::move(tasks[i]);
			};
			EXPECT_TRUE(test::run_until_done(context, all(context, server->local_address(), std::move(requests), behaviour, results)));
			return results;
		}

		auto run(tftp_packet request, client_behaviour behaviour = {}) -> download_result
		{
			return run(std::vector<tftp_packet>{ std::move(request) }, behaviour).front();
		}

		test::temp_directory dir;
		std::filesystem::path root;
		std::optional<tftp_server_v4> server;
		async::io_context context;
	};
}

TEST_F(tftp_server_test, sends_a_file)
{
	const auto result = run(tftp_packet::make_rrq("small.bin", "octet", {}));
	EXPECT_FALSE(result.error.has_value());
	EXPECT_FALSE(result.oack.has_value());
	EXPECT_EQ(result.data, test::pattern(1300));
}

TEST_F(tftp_server_test, negotiates_options)
{
	const auto result = run(tftp_packet::make_rrq("exact.bin", "octet", { { "blksize", "1024" }, { "tsize", "0" } }));
	ASSERT_TRUE(result.oack.has_value());
	EXPECT_EQ(result.oack->at("blksize"), "1024");
	EXPECT_EQ(result.oack->at("tsize"), "2048");
	EXPECT_EQ(result.data, test::pattern(2048));
	EXPECT_EQ(result.data_packets, 3u); // 1024 + 1024 + empty final block
}

TEST_F(tftp_server_test, ignores_invalid_options)
{
	const auto result = run(tftp_packet::make_rrq("small.bin", "octet", { { "blksize", "0" } }));
	EXPECT_FALSE(result.oack.has_value());
	EXPECT_EQ(result.data, test::pattern(1300));
}

TEST_F(tftp_server_test, serves_files_in_subdirectories)
{
	const auto results = run({
		tftp_packet::make_rrq("sub/dir/file.txt", "netascii", {}),
		tftp_packet::make_rrq("\\sub\\dir\\file.txt", "octet", {}),
	});
	EXPECT_EQ(results[0].data, test::bytes("hello"));
	EXPECT_EQ(results[1].data, test::bytes("hello"));
}

TEST_F(tftp_server_test, reports_missing_files)
{
	const auto result = run(tftp_packet::make_rrq("missing.bin", "octet", {}));
	ASSERT_TRUE(result.error.has_value());
	EXPECT_EQ(result.error->error_code, tftp_packet::file_not_found);
}

TEST_F(tftp_server_test, refuses_paths_outside_the_root)
{
	const auto result = run(tftp_packet::make_rrq("../secret.txt", "octet", {}));
	ASSERT_TRUE(result.error.has_value());
	EXPECT_EQ(result.error->error_code, tftp_packet::access_violation);
}

TEST_F(tftp_server_test, refuses_uploads)
{
	const auto result = run(tftp_packet::make_wrq("upload.bin", "octet", {}));
	ASSERT_TRUE(result.error.has_value());
	EXPECT_EQ(result.error->error_code, tftp_packet::access_violation);
}

TEST_F(tftp_server_test, refuses_unknown_modes)
{
	const auto result = run(tftp_packet::make_rrq("small.bin", "mail", {}));
	ASSERT_TRUE(result.error.has_value());
	EXPECT_EQ(result.error->error_code, tftp_packet::illegal_operation);
}

TEST_F(tftp_server_test, retransmits_after_a_timeout)
{
	const auto started = std::chrono::steady_clock::now();
	const auto result = run(tftp_packet::make_rrq("small.bin", "octet", {}), { .ignore_first_data = true });
	EXPECT_EQ(result.data, test::pattern(1300));
	EXPECT_EQ(result.data_packets, 4u);
	EXPECT_GE(std::chrono::steady_clock::now() - started, 900ms);
}

TEST_F(tftp_server_test, survives_duplicate_acks)
{
	// Regression : a duplicate ACK used to abort the transfer.
	const auto result = run(tftp_packet::make_rrq("small.bin", "octet", {}), { .duplicate_acks = true });
	EXPECT_FALSE(result.error.has_value());
	EXPECT_EQ(result.data, test::pattern(1300));
	// Duplicate ACKs must not trigger retransmissions (Sorcerer's Apprentice).
	EXPECT_EQ(result.retransmissions, 0u);
}

TEST_F(tftp_server_test, serves_concurrent_transfers)
{
	const auto results = run({
		tftp_packet::make_rrq("small.bin", "octet", {}),
		tftp_packet::make_rrq("exact.bin", "octet", { { "blksize", "512" } }),
		tftp_packet::make_rrq("sub/dir/file.txt", "octet", {}),
	});
	EXPECT_EQ(results[0].data, test::pattern(1300));
	EXPECT_EQ(results[1].data, test::pattern(2048));
	EXPECT_EQ(results[2].data, test::bytes("hello"));
}

TEST(tftp_server, requires_an_existing_root)
{
	tftp_server_v4 server(test::config_from("tftp_base_dir = /definitely/not/here\ntftp_listen_port = 0\n"));
	EXPECT_THROW(server.bind(), std::runtime_error);
}
