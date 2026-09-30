#include "async.hpp"
#include "logger.hpp"

#include <algorithm>

namespace async
{
	// The coroutine type wrapping a spawned task. It starts suspended (the
	// context schedules it) and destroys itself once finished, removing its
	// entry from io_context::m_tasks.
	struct io_context::detached
	{
		struct promise_type
		{
			struct final_awaiter
			{
				bool await_ready() const noexcept { return false; }

				void await_suspend(std::coroutine_handle<promise_type> self) noexcept
				{
					auto& promise_v = self.promise();
					promise_v.m_context->m_tasks.erase(promise_v.m_self);
					self.destroy();
				}

				void await_resume() const noexcept {}
			};

			auto get_return_object() noexcept -> detached
			{ return detached{ std::coroutine_handle<promise_type>::from_promise(*this) }; }

			auto initial_suspend() const noexcept -> std::suspend_always { return {}; }
			auto final_suspend() const noexcept -> final_awaiter { return {}; }
			void return_void() const noexcept {}
			void unhandled_exception() const noexcept { std::terminate(); }

			io_context* m_context{ nullptr };
			std::list<std::coroutine_handle<>>::iterator m_self;
		};

		std::coroutine_handle<promise_type> m_handle;
	};

	auto io_context::run_detached(task<void> work) -> detached
	{
		try
		{
			co_await std::move(work);
		}
		catch (operation_cancelled const&)
		{}
		catch (std::exception const& ex)
		{
			Glog.error("Unhandled exception in task : {}", ex.what());
		}
		catch (...)
		{
			Glog.error("Unhandled unknown exception in task.");
		}
	}

	io_context::io_context() = default;

	io_context::~io_context()
	{
		// Only reached with live tasks if run() wasn't called or didn't finish.
		// Destroying the top level frames destroys the whole await chains.
		m_waiters.clear();
		m_ready.clear();
		while (!m_tasks.empty())
		{
			auto handle = m_tasks.front();
			m_tasks.pop_front();
			handle.destroy();
		}
	}

	void io_context::spawn(task<void> work)
	{
		auto detached_v = run_detached(std::move(work));
		auto& promise_v = detached_v.m_handle.promise();
		promise_v.m_context = this;
		promise_v.m_self = m_tasks.insert(m_tasks.end(), detached_v.m_handle);
		m_ready.push_back(detached_v.m_handle);
	}

	void io_context::run(std::function<bool()> should_stop)
	{
		while (!m_tasks.empty())
		{
			if (should_stop && !stopping() && should_stop())
				stop();

			if (stopping())
				cancel_waiters();

			while (!m_ready.empty())
			{
				auto handle = m_ready.front();
				m_ready.pop_front();
				handle.resume();
			}

			if (m_tasks.empty())
				break;

			if (!stopping())
				poll_once();
		}
	}

	void io_context::stop() noexcept
	{
		m_stop.store(true);
	}

	auto io_context::stopping() const noexcept -> bool
	{
		return m_stop.load();
	}

	auto io_context::task_count() const noexcept -> std::size_t
	{
		return m_tasks.size();
	}

	void io_context::cancel_waiters()
	{
		for (auto* waiter_v : m_waiters)
		{
			waiter_v->m_status = wait_awaiter::status::cancelled;
			m_ready.push_back(waiter_v->m_handle);
		}
		m_waiters.clear();
	}

	void io_context::poll_once()
	{
		using namespace std::chrono;
		using status = wait_awaiter::status;

		auto now = clock::now();
		auto wake_up = now + max_poll_interval;

		std::vector<socket_poll_entry> entries;
		std::vector<std::size_t> owners;
		for (std::size_t i = 0; i < m_waiters.size(); ++i)
		{
			wake_up = std::min(wake_up, m_waiters[i]->m_deadline);
			if (m_waiters[i]->m_socket != v4_socket_make_invalid()) {
				entries.push_back({ m_waiters[i]->m_socket });
				owners.push_back(i);
			}
		}

		const auto timeout_ms = wake_up > now
			? (int)ceil<milliseconds>(wake_up - now).count()
			: 0;
		v4_socket_poll(entries, timeout_ms);
		now = clock::now();

		for (std::size_t k = 0; k < entries.size(); ++k)
			if (entries[k].readable)
				m_waiters[owners[k]]->m_status = status::readable;

		std::vector<wait_awaiter*> still_waiting;
		for (auto* waiter_v : m_waiters)
		{
			if (waiter_v->m_status == status::pending && waiter_v->m_deadline <= now)
				waiter_v->m_status = status::timed_out;

			if (waiter_v->m_status == status::pending)
				still_waiting.push_back(waiter_v);
			else
				m_ready.push_back(waiter_v->m_handle);
		}
		m_waiters = std::move(still_waiting);
	}

	bool io_context::wait_awaiter::await_ready()
	{
		if (m_context.stopping()) {
			m_status = status::cancelled;
			return true;
		}
		return false;
	}

	void io_context::wait_awaiter::await_suspend(std::coroutine_handle<> handle)
	{
		m_handle = handle;
		m_context.m_waiters.push_back(this);
	}

	auto io_context::wait_awaiter::await_resume() -> bool
	{
		if (m_status == status::cancelled)
			throw operation_cancelled{};
		return m_status == status::readable;
	}

	auto io_context::wait_readable(int_socket_type socket, time_point deadline) -> wait_awaiter
	{
		return wait_awaiter{ *this, socket, deadline, wait_awaiter::status::pending, {} };
	}

	auto io_context::sleep_until(time_point deadline) -> wait_awaiter
	{
		return wait_awaiter{ *this, v4_socket_make_invalid(), deadline, wait_awaiter::status::pending, {} };
	}

	auto io_context::sleep_for(duration timeout) -> wait_awaiter
	{
		return sleep_until(clock::now() + timeout);
	}
}
