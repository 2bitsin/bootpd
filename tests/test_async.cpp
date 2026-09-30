#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <common/async.hpp>
#include <common/socket_udp.hpp>

#include "test_helpers.hpp"

using namespace std::chrono_literals;

namespace
{
	auto answer() -> async::task<int> { co_return 42; }

	auto add(int a, int b) -> async::task<int>
	{
		const auto x = co_await answer();
		co_return a + b + x;
	}

	auto fail() -> async::task<int>
	{
		throw std::runtime_error("boom");
		co_return 0;
	}

	auto record(std::vector<int>& log, async::io_context& context, int id, std::chrono::milliseconds delay) -> async::task<void>
	{
		co_await context.sleep_for(delay);
		log.push_back(id);
	}

	// Deep chains of synchronously completing awaits must not grow the stack.
	auto countdown(int n) -> async::task<int>
	{
		if (n == 0)
			co_return 0;
		co_return 1 + co_await countdown(n - 1);
	}
}

TEST(async_task, returns_values_through_await_chains)
{
	async::io_context context;
	int result = 0;
	const auto body = [](int& result) -> async::task<void> { result = co_await add(1, 2); };
	context.spawn(body(result));
	context.run();
	EXPECT_EQ(result, 45);
	EXPECT_EQ(context.task_count(), 0u);
}

TEST(async_task, propagates_exceptions_to_the_awaiter)
{
	async::io_context context;
	std::string message;
	const auto body = [](std::string& message) -> async::task<void>
	{
		try { co_await fail(); }
		catch (std::runtime_error const& ex) { message = ex.what(); }
	};
	context.spawn(body(message));
	context.run();
	EXPECT_EQ(message, "boom");
}

TEST(async_task, handles_deep_recursion)
{
	async::io_context context;
	int result = 0;
	const auto body = [](int& result) -> async::task<void> { result = co_await countdown(10000); };
	context.spawn(body(result));
	context.run();
	EXPECT_EQ(result, 10000);
}

TEST(async_task, unstarted_task_is_destroyed_cleanly)
{
	auto work = answer();
	EXPECT_TRUE(work.valid());
}

TEST(io_context, orders_timers_by_deadline)
{
	async::io_context context;
	std::vector<int> log;
	context.spawn(record(log, context, 3, 60ms));
	context.spawn(record(log, context, 1, 10ms));
	context.spawn(record(log, context, 2, 30ms));
	const auto started = std::chrono::steady_clock::now();
	context.run();
	EXPECT_EQ(log, (std::vector<int>{ 1, 2, 3 }));
	EXPECT_GE(std::chrono::steady_clock::now() - started, 60ms);
}

TEST(io_context, an_escaping_exception_only_ends_its_own_task)
{
	async::io_context context;
	std::vector<int> log;
	const auto thrower = []() -> async::task<void> { co_await fail(); };
	context.spawn(thrower());
	context.spawn(record(log, context, 1, 5ms));
	context.run();
	EXPECT_EQ(log, (std::vector<int>{ 1 }));
}

TEST(io_context, stop_cancels_pending_waits)
{
	async::io_context context;
	bool cancelled = false;
	bool finished = false;
	const auto sleeper = [](async::io_context& context, bool& cancelled, bool& finished) -> async::task<void>
	{
		try { co_await context.sleep_for(1h); finished = true; }
		catch (async::operation_cancelled const&) { cancelled = true; throw; }
	};
	const auto stopper = [](async::io_context& context) -> async::task<void>
	{
		co_await context.sleep_for(10ms);
		context.stop();
	};
	context.spawn(sleeper(context, cancelled, finished));
	context.spawn(stopper(context));
	const auto started = std::chrono::steady_clock::now();
	context.run();
	EXPECT_TRUE(cancelled);
	EXPECT_FALSE(finished);
	EXPECT_LT(std::chrono::steady_clock::now() - started, 5s);
	EXPECT_EQ(context.task_count(), 0u);
}

TEST(io_context, external_stop_predicate_stops_the_loop)
{
	async::io_context context;
	const auto sleeper = [](async::io_context& context) -> async::task<void> { co_await context.sleep_for(1h); };
	context.spawn(sleeper(context));
	const auto deadline = std::chrono::steady_clock::now() + 50ms;
	context.run([deadline] { return std::chrono::steady_clock::now() > deadline; });
	EXPECT_TRUE(context.stopping());
	EXPECT_EQ(context.task_count(), 0u);
}

TEST(io_context, destroys_unfinished_tasks)
{
	auto context = std::make_unique<async::io_context>();
	const auto sleeper = [](async::io_context& context) -> async::task<void> { co_await context.sleep_for(1h); };
	context->spawn(sleeper(*context));
	EXPECT_EQ(context->task_count(), 1u);
	context.reset();
}

TEST(socket_udp, receives_datagrams_asynchronously)
{
	async::io_context context;
	socket_udp receiver(address_v4::loopback(0));
	socket_udp sender(address_v4::loopback(0));
	std::optional<datagram> received;
	bool timed_out = false;

	const auto body = [](async::io_context& context, socket_udp& receiver, socket_udp& sender,
		std::optional<datagram>& received, bool& timed_out) -> async::task<void>
	{
		const auto empty = co_await receiver.async_recv(context, 20ms);
		timed_out = !empty.has_value();
		const auto payload = test::bytes("hello");
		sender.send(payload, receiver.local_address());
		received = co_await receiver.async_recv(context, 2s);
	};
	context.spawn(body(context, receiver, sender, received, timed_out));
	context.run();

	EXPECT_TRUE(timed_out);
	ASSERT_TRUE(received.has_value());
	EXPECT_EQ(received->data, test::bytes("hello"));
	EXPECT_EQ(received->source, sender.local_address());
}
