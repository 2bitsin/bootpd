#pragma once

#include <charconv>
#include <cstdint>
#include <exception>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

struct bad_lexical_cast
: public std::exception
{
	bad_lexical_cast() noexcept
	{}

	bad_lexical_cast(std::string_view message)
	: m_message(message)
	{}

	const char* what() const noexcept override
	{ return m_message.c_str(); }

private:
	std::string m_message;
};

// Converts text to a value of type T.
//
// * bool          : true/false, yes/no, on/off, 1/0 (case insensitive)
// * integers      : decimal, or 0x / 0o / 0b prefixed; range checked for T
// * floating point: std::from_chars syntax
// * anything else : constructed from the string_view (e.g. std::string, paths)
template <typename T>
auto lexical_cast(std::string_view what) -> T
{
	using namespace std::string_literals;

	if constexpr (std::is_same_v<T, bool>)
	{
		std::string lower_v;
		for (auto c : what)
			lower_v.push_back((char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c));
		if (lower_v == "true" || lower_v == "yes" || lower_v == "on" || lower_v == "1")
			return true;
		if (lower_v == "false" || lower_v == "no" || lower_v == "off" || lower_v == "0")
			return false;
		throw bad_lexical_cast("Invalid boolean value: "s + std::string(what));
	}
	else if constexpr (std::is_integral_v<T>)
	{
		auto digits = what;
		bool negative = false;
		if (digits.starts_with('-')) {
			negative = true;
			digits.remove_prefix(1);
		} else if (digits.starts_with('+')) {
			digits.remove_prefix(1);
		}

		int base = 10;
		if (digits.starts_with("0x") || digits.starts_with("0X")) {
			digits.remove_prefix(2);
			base = 16;
		} else if (digits.starts_with("0o") || digits.starts_with("0O")) {
			digits.remove_prefix(2);
			base = 8;
		} else if (digits.starts_with("0b") || digits.starts_with("0B")) {
			digits.remove_prefix(2);
			base = 2;
		}

		std::uintmax_t magnitude = 0;
		const auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), magnitude, base);
		if (digits.empty() || ec != std::errc{} || ptr != digits.data() + digits.size())
			throw bad_lexical_cast("Invalid integer value: "s + std::string(what));

		if constexpr (std::is_unsigned_v<T>)
		{
			if (negative && magnitude != 0)
				throw bad_lexical_cast("Value must not be negative: "s + std::string(what));
			if (magnitude > std::numeric_limits<T>::max())
				throw bad_lexical_cast("Value out of range: "s + std::string(what));
			return static_cast<T>(magnitude);
		}
		else
		{
			using U = std::make_unsigned_t<T>;
			const auto limit = negative
				? static_cast<std::uintmax_t>(static_cast<U>(std::numeric_limits<T>::max())) + 1u
				: static_cast<std::uintmax_t>(std::numeric_limits<T>::max());
			if (magnitude > limit)
				throw bad_lexical_cast("Value out of range: "s + std::string(what));
			return (negative && magnitude != 0u)
				? static_cast<T>(-static_cast<std::intmax_t>(magnitude - 1u) - 1)
				: static_cast<T>(magnitude);
		}
	}
	else if constexpr (std::is_floating_point_v<T>)
	{
		T value{};
		const auto [ptr, ec] = std::from_chars(what.data(), what.data() + what.size(), value);
		if (what.empty() || ec != std::errc{} || ptr != what.data() + what.size())
			throw bad_lexical_cast("Invalid floating point value: "s + std::string(what));
		return value;
	}
	else
	{
		return T(what);
	}
}
