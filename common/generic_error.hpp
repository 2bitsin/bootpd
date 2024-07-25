#pragma once

#include <string_view>
#include <format>

using format_error_t = struct {};

static inline constexpr format_error_t format_error {};

struct generic_error: std::exception
{
	virtual unsigned int code () const = 0;
};

template<unsigned int Error_code, template <unsigned int> typename Error_getter>
struct generic_error_t: generic_error
{
	using error_getter = Error_getter<Error_code>;
	
	generic_error_t(const generic_error_t&) = default;
	generic_error_t(generic_error_t&&) = default;
	
	static constexpr const inline unsigned int value = Error_code;

	template <typename... Args>	
	generic_error_t(format_error_t, Args&&... args)
	:	m_error_string 
		{ 
			std::format(error_getter::value(), 
								 std::forward<Args>(args)...)
		}
	{}
	
	constexpr unsigned int code() const override { return Error_code; }
	const char* what () const override { return m_error_string.data(); }

private:
	std::string m_error_string;
};
