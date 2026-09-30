#pragma once

// Graceful shutdown on Ctrl+C (and SIGTERM / console close).
struct control_c
{
	// Installs the handler. Safe to call more than once.
	static void install();
	static auto stop_requested() noexcept -> bool;
	static void request_stop() noexcept;
};
