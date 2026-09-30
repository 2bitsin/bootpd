#include "logger.hpp"

#include <chrono>
#include <ctime>
#include <iostream>
#include <string>

auto basic_logger::filter_level_to_string(filter_level level) -> std::string
{
	using namespace std::string_literals;
	switch (level)
	{
	using enum filter_level;
	case fatal:   return "FATAL  "s;
	case error:   return "ERROR  "s;
	case warning: return "WARNING"s;
	case info:    return "INFO   "s;
	case debug:   return "DEBUG  "s;
	case trace:   return "TRACE  "s;
	default:      return "-------"s;
	}
}

auto basic_logger::datetime_as_string() -> std::string
{
	using namespace std::chrono;
	const auto now = system_clock::to_time_t(system_clock::now());
	std::tm local_tm{};
#ifdef _WIN32
	localtime_s(&local_tm, &now);
#else
	localtime_r(&now, &local_tm);
#endif
	char buffer[32]{};
	const auto length = std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
	return std::string(buffer, length);
}

basic_logger::basic_logger(logger_sink_fun sink)
:	m_sink(std::move(sink))
{}

basic_logger::basic_logger(std::ostream& error_sink_s, std::ostream& info_sink_s)
:	basic_logger {[&error_sink_s, &info_sink_s] (filter_level level, std::string_view line)
	{
		auto& stream_v = (level < filter_level::info) ? error_sink_s : info_sink_s;
		stream_v.write(line.data(), (std::streamsize)line.size());
		stream_v.flush();
	}}
{}

void basic_logger::level(filter_level level) noexcept
{
	m_level = level;
}

auto basic_logger::level() const noexcept -> filter_level
{
	return m_level;
}

auto basic_logger::enabled(filter_level level) const noexcept -> bool
{
	return level <= m_level;
}

auto basic_logger::sink(logger_sink_fun sink) -> logger_sink_fun
{
	std::lock_guard lock(m_mutex);
	return std::exchange(m_sink, std::move(sink));
}

void basic_logger::sink_line(filter_level level, std::string_view line)
{
	auto text_v = std::format("{} {} {}\n", datetime_as_string(), filter_level_to_string(level), line);
	std::lock_guard lock(m_mutex);
	if (m_sink)
		m_sink(level, text_v);
}

namespace
{
	struct console_logger
	: public basic_logger
	{
		console_logger()
		: basic_logger(std::cerr, std::cout)
		{}
	};
}

// Constructed on first use, so it is safe to log from other static initializers.
static auto console() -> basic_logger&
{
	static console_logger instance;
	return instance;
}

basic_logger& Glog = console();
