#include "logger.hpp"

#include <chrono>
#include <string>
#include <ctime>

auto basic_logger::filter_level_to_string(filter_level level) -> std::string
{
	using namespace std::string_literals;
	switch (level)
	{
	using enum filter_level;
	case fatal:		return "FATAL  "s;
	case error:		return "ERROR  "s;
	case warning:	return "WARNING"s;
	case info:		return "INFO   "s;
	case debug:		return "DEBUG  "s;
	case trace:		return "TRACE  "s;
	default:      return "-------"s;
	}
}

auto basic_logger::datetime_as_string() -> std::string
{
	using namespace std;
	using namespace chrono;
  auto now = system_clock::to_time_t(system_clock::now());
  string s(30, '\0');
  strftime(s.data(), s.size(), "%Y-%m-%d %H:%M:%S", localtime(&now));
  return s;
}

basic_logger::basic_logger(logger_sink_fun sink)
:	m_sink(std::move(sink))
{}

basic_logger::basic_logger(std::ostream& error_sink_s, std::ostream& info_sink_s)
:	basic_logger {[&error_sink_s, &info_sink_s] (filter_level level, std::string_view line) 
	{
		switch(level)
		{
		using enum filter_level;
		case fatal:
		case error:
		case warning:
			error_sink_s.write(line.data(), line.size());
			break;
		case info:
		case debug:
		case trace:
			info_sink_s.write(line.data(), line.size());
			break;
		}
	}}
{}

void basic_logger::sink_line(filter_level level, std::string_view line)
{
	using namespace std::string_literals;
	m_sink(level, datetime_as_string() + " "s + filter_level_to_string(level) + " "s + std::string(line) + "\n"s);
}

#include <iostream>
struct console_logger
: public basic_logger
{
	console_logger()
	: basic_logger(std::cerr, std::cout) 
	{}
};

static console_logger _Console_logger;
basic_logger& Glog = _Console_logger;