#pragma once

#include <string_view>
#include <string>
#include <type_traits>
#include <typeinfo>

struct bad_lexical_cast
: public std::exception 
{
	bad_lexical_cast() noexcept 
	{}

	bad_lexical_cast(std::string_view message) noexcept
	: m_message(message) 
	{}

	const char* what() const noexcept override 
	{ return m_message.c_str(); }
	
private:
	std::string m_message;
};

template <typename T>
auto lexical_cast(std::string_view what) -> T
{
	using namespace std::string_literals;
	using namespace std::string_view_literals;
	if constexpr (std::is_arithmetic_v<T>)
	{
		T value = T();
		size_t offs = 0;
		if constexpr (std::is_integral_v<T>)
		{
			int base = 10;			
			if (what.starts_with("0x") || what.starts_with("0X")) {
				what = what.substr(2);
				base = 16;
			} else 
			if (what.starts_with("0o") || what.starts_with("0O")) {
				what = what.substr(2);
				base = 8;
			} else 
			if (what.starts_with("0b") || what.starts_with("0b")) {
				what = what.substr(2);
				base = 2;
			}
			
			if constexpr (std::is_unsigned_v<T>)
			{
				if (sizeof(T) <= sizeof(unsigned long))		
					value = static_cast<T>(std::stoul(std::string(what), &offs, base));			
				else
					value = static_cast<T>(std::stoull(std::string(what), &offs, base));

			} else
			if constexpr (std::is_signed_v<T>)
			{
				if (sizeof(T) <= sizeof(long))		
					value = static_cast<T>(std::stol(std::string(what), &offs, base));			
				else
					value = static_cast<T>(std::stoll(std::string(what), &offs, base));

			}
		} else		
		if constexpr (std::is_floating_point_v<T>)
		{	
			if (sizeof(T) <= sizeof(float))		
				value = static_cast<T>(std::stof(std::string(what), &offs));			
			else
				value = static_cast<T>(std::stod(std::string(what), &offs));
		} 
		if (offs != what.size()) {
			throw bad_lexical_cast("Invalid value: "s + std::string(what));
		}
		
		return value;
	} else
	if constexpr (std::is_same_v<T, bool>)
	{
		if (what == "true"sv || what == "True"sv || what == "TRUE"sv) {
			return true;
		}

		if (what == "false"sv || what == "False"sv || what == "FALSE"sv) {
			return false;
		}
		
		throw bad_lexical_cast("Invalid boolean value : "s + std::string(what));		
	}
	else
	{
		return T(what);
	}
}