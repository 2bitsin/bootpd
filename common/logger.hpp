#pragma once

#include <format>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

struct basic_logger
{
	enum class filter_level: int
	{
		fatal   = -3,
		error   = -2,
		warning = -1,
		info    = +0,
		debug   = +1,
		trace   = +2
	};

	using logger_sink_fun = std::function<void(filter_level, std::string_view)>;

	// Format strings are checked at compile time (std::format_string).
	template <typename ... Args>
	auto write_line(filter_level level, std::format_string<Args...> fmt, Args&& ... args)
		-> basic_logger&
	{
		if (!enabled(level))
			return *this;
		sink_line(level, std::format(fmt, std::forward<Args>(args)...));
		return *this;
	}

#define DEFINE_LOGGER_SHORTCUT(X)                                                  \
	template <typename ... Args>                                                     \
	auto X(std::format_string<Args...> fmt, Args&& ... args) -> basic_logger&        \
	{                                                                                \
		return write_line(filter_level:: X, fmt, std::forward<Args>(args)...);         \
	}

	DEFINE_LOGGER_SHORTCUT(fatal)
	DEFINE_LOGGER_SHORTCUT(error)
	DEFINE_LOGGER_SHORTCUT(warning)
	DEFINE_LOGGER_SHORTCUT(info)
	DEFINE_LOGGER_SHORTCUT(debug)
	DEFINE_LOGGER_SHORTCUT(trace)

#undef DEFINE_LOGGER_SHORTCUT

	// Messages more verbose than `level` are discarded.
	void level(filter_level level) noexcept;
	auto level() const noexcept -> filter_level;
	auto enabled(filter_level level) const noexcept -> bool;

	// Replaces the output sink, returns the previous one (useful for tests).
	auto sink(logger_sink_fun sink) -> logger_sink_fun;

	static auto filter_level_to_string(filter_level level) -> std::string;

protected:

	static auto datetime_as_string() -> std::string;

	basic_logger(logger_sink_fun sink);
	basic_logger(std::ostream& error_sink_s, std::ostream& info_sink_s);
	~basic_logger() = default;

	void sink_line(filter_level level, std::string_view);

private:
	std::mutex m_mutex;
	logger_sink_fun m_sink;
	filter_level m_level{ filter_level::info };
};

extern basic_logger& Glog;
