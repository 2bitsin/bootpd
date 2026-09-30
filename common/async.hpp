#pragma once

// A minimal single-threaded coroutine runtime.
//
//  * task<T>     - a lazily started coroutine that produces a T (or throws).
//                  Awaiting it starts it; the awaiter is resumed by symmetric
//                  transfer when it finishes, so deep await chains don't grow
//                  the stack.
//  * io_context  - the event loop. It owns detached ("spawned") top level
//                  tasks, and resumes coroutines waiting for a socket to become
//                  readable and/or for a deadline to pass.
//
// Everything runs on the thread that calls io_context::run(); no locking is
// needed anywhere in the servers. Only io_context::stop() may be called from
// other threads or signal handlers.

#include <atomic>
#include <chrono>
#include <coroutine>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <list>
#include <optional>
#include <utility>
#include <vector>

#include "socket_api.hpp"

namespace async
{
	// Thrown out of every pending (and any new) wait once io_context::stop()
	// was called. Deliberately *not* derived from std::exception, so that the
	// usual `catch (std::exception const&)` error handlers in server loops
	// don't swallow it and the tasks actually unwind.
	struct operation_cancelled
	{
		const char* what() const noexcept { return "operation cancelled"; }
	};

	template <typename T = void>
	class task;

	namespace detail
	{
		struct promise_base
		{
			struct final_awaiter
			{
				bool await_ready() const noexcept { return false; }

				template <typename Promise>
				auto await_suspend(std::coroutine_handle<Promise> self) noexcept -> std::coroutine_handle<>
				{ return self.promise().m_continuation; }

				void await_resume() const noexcept {}
			};

			auto initial_suspend() const noexcept -> std::suspend_always { return {}; }
			auto final_suspend() const noexcept -> final_awaiter { return {}; }
			void unhandled_exception() noexcept { m_exception = std::current_exception(); }

			std::coroutine_handle<> m_continuation{ std::noop_coroutine() };
			std::exception_ptr m_exception;
		};

		template <typename T>
		struct promise: promise_base
		{
			auto get_return_object() noexcept -> task<T>;

			template <typename U>
			void return_value(U&& value) { m_value.emplace(std::forward<U>(value)); }

			auto result() -> T
			{
				if (m_exception)
					std::rethrow_exception(m_exception);
				return std::move(*m_value);
			}

			std::optional<T> m_value;
		};

		template <>
		struct promise<void>: promise_base
		{
			auto get_return_object() noexcept -> task<void>;

			void return_void() const noexcept {}

			void result()
			{
				if (m_exception)
					std::rethrow_exception(m_exception);
			}
		};
	}

	template <typename T>
	class [[nodiscard]] task
	{
	public:
		using promise_type = detail::promise<T>;
		using handle_type = std::coroutine_handle<promise_type>;

		task() noexcept = default;
		explicit task(handle_type handle) noexcept: m_handle{ handle } {}
		task(task&& other) noexcept: m_handle{ std::exchange(other.m_handle, {}) } {}
		task(task const&) = delete;
		auto operator = (task const&) -> task& = delete;

		auto operator = (task&& other) noexcept -> task&
		{
			if (this != &other) {
				destroy();
				m_handle = std::exchange(other.m_handle, {});
			}
			return *this;
		}

		~task() { destroy(); }

		auto valid() const noexcept -> bool { return (bool)m_handle; }

		auto operator co_await() && noexcept
		{
			struct awaiter
			{
				handle_type m_handle;

				bool await_ready() const noexcept { return m_handle.done(); }

				auto await_suspend(std::coroutine_handle<> awaiting) noexcept -> std::coroutine_handle<>
				{
					m_handle.promise().m_continuation = awaiting;
					return m_handle;
				}

				auto await_resume() -> T { return m_handle.promise().result(); }
			};
			return awaiter{ m_handle };
		}

	private:
		void destroy() noexcept
		{
			if (m_handle)
				std::exchange(m_handle, {}).destroy();
		}

		handle_type m_handle;
	};

	namespace detail
	{
		template <typename T>
		inline auto promise<T>::get_return_object() noexcept -> task<T>
		{ return task<T>{ std::coroutine_handle<promise<T>>::from_promise(*this) }; }

		inline auto promise<void>::get_return_object() noexcept -> task<void>
		{ return task<void>{ std::coroutine_handle<promise<void>>::from_promise(*this) }; }
	}

	class io_context
	{
	public:
		using clock = std::chrono::steady_clock;
		using time_point = clock::time_point;
		using duration = clock::duration;

		// The longest time the loop blocks without re-checking the external stop predicate.
		static constexpr auto max_poll_interval = std::chrono::milliseconds(100);

		io_context();
		io_context(io_context const&) = delete;
		auto operator = (io_context const&) -> io_context& = delete;
		~io_context();

		// Starts `work` as an independent top level task, owned by the context.
		// Exceptions escaping it are logged, cancellation is silent.
		void spawn(task<void> work);

		// Runs the loop until every spawned task has finished. If given,
		// `should_stop` is polled regularly and triggers stop() when it
		// returns true.
		void run(std::function<bool()> should_stop = {});

		// Cancels every pending and future wait, which makes the tasks unwind.
		// Safe to call from any thread and from signal handlers.
		void stop() noexcept;
		auto stopping() const noexcept -> bool;

		// Number of spawned tasks that haven't finished yet.
		auto task_count() const noexcept -> std::size_t;

		// Awaitable: suspends until `socket` is readable (-> true) or `deadline` passes (-> false).
		struct wait_awaiter
		{
			bool await_ready();
			void await_suspend(std::coroutine_handle<> handle);
			auto await_resume() -> bool;

			enum class status { pending, readable, timed_out, cancelled };

			io_context& m_context;
			int_socket_type m_socket;
			time_point m_deadline;
			status m_status{ status::pending };
			std::coroutine_handle<> m_handle;
		};

		auto wait_readable(int_socket_type socket, time_point deadline) -> wait_awaiter;
		auto sleep_until(time_point deadline) -> wait_awaiter;
		auto sleep_for(duration timeout) -> wait_awaiter;

	private:
		struct detached;
		static auto run_detached(task<void> work) -> detached;

		void cancel_waiters();
		void poll_once();

		std::deque<std::coroutine_handle<>> m_ready;
		std::vector<wait_awaiter*> m_waiters;
		std::list<std::coroutine_handle<>> m_tasks;
		std::atomic<bool> m_stop{ false };
	};
}
