#include "control_c.hpp"

#include <atomic>
#include <mutex>
#include <stdexcept>

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
#else
	#include <csignal>
#endif

namespace
{
	// Lock free, so it may be written from a signal handler.
	std::atomic<bool> G_stop_requested{ false };
	static_assert(std::atomic<bool>::is_always_lock_free);

#ifdef _WIN32
	BOOL WINAPI control_c_handler(DWORD event)
	{
		switch (event)
		{
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			G_stop_requested.store(true);
			return TRUE;
		default:
			return FALSE;
		}
	}
#else
	void control_c_handler(int)
	{
		G_stop_requested.store(true);
	}
#endif
}

void control_c::install()
{
	static std::once_flag once;
	std::call_once(once, []()
	{
#ifdef _WIN32
		if (!SetConsoleCtrlHandler(control_c_handler, TRUE))
			throw std::runtime_error("Unable to install control+c handler.");
#else
		struct sigaction action{};
		action.sa_handler = control_c_handler;
		sigemptyset(&action.sa_mask);
		if (sigaction(SIGINT, &action, nullptr) != 0 || sigaction(SIGTERM, &action, nullptr) != 0)
			throw std::runtime_error("Unable to install SIGINT/SIGTERM handler.");
#endif
	});
}

auto control_c::stop_requested() noexcept -> bool
{
	return G_stop_requested.load();
}

void control_c::request_stop() noexcept
{
	G_stop_requested.store(true);
}
