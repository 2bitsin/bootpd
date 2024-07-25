#pragma once

#include <functional>
#include <string_view>
#include <string>
#include <format>

struct basic_logger
{	
	enum class filter_level: int
	{
		fatal   = -3,
		error   = -2, 
		warning = -1, 
		info		= +0, 
		debug		= +1, 
		trace		= +2
	};
	
	using logger_sink_fun = std::function<void(filter_level, std::string_view)>;	
	
	template <typename ... Args>
	auto write_line(filter_level level, const std::string_view fmt, Args ... args)
		-> basic_logger&
	{		
		using namespace std::string_literals;
		auto formatted_line = std::vformat(fmt, std::make_format_args(args...));
		sink_line(level, formatted_line);
		return *this;
	}

#define DEFINE_LOGGER_SHORTCUT(X)                                                                \
	template <typename ... Args>                                                  \
	auto X(const std::string_view fmt, Args&& ... args) -> basic_logger&                                          \
	{                                                                                              \
		return write_line(filter_level:: X, fmt, std::forward<Args>(args)...); \
	}

	DEFINE_LOGGER_SHORTCUT(fatal)
	DEFINE_LOGGER_SHORTCUT(error)
	DEFINE_LOGGER_SHORTCUT(warning)
	DEFINE_LOGGER_SHORTCUT(info)
	DEFINE_LOGGER_SHORTCUT(debug)
	DEFINE_LOGGER_SHORTCUT(trace)
	
#undef DEFINE_LOGGER_SHORTCUT
			
protected:

	static auto filter_level_to_string(filter_level level) -> std::string;
	static auto datetime_as_string() -> std::string;
	
	basic_logger(logger_sink_fun sink);
	basic_logger(std::ostream& error_sink_s, std::ostream& info_sink_s);
	
#if defined(LOGGER_VIRTUAL) && (LOGGER_VIRTUAL != 0)
	#define LOGGER_MAYBE_VIRTUAL virtual
#else
	#define LOGGER_MAYBE_VIRTUAL 
#endif
	
	LOGGER_MAYBE_VIRTUAL
	void sink_line(filter_level level, std::string_view);		
	
	LOGGER_MAYBE_VIRTUAL
	~basic_logger() = default;

#undef LOGGER_MAYBE_VIRTUAL
	
private:
	logger_sink_fun m_sink;
};


extern basic_logger& Glog;