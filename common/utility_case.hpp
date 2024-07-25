#pragma once

#include <algorithm>
#include <string>
#include <cctype>
#include <iterator>
#include <string_view>

static inline constexpr auto lowercase(std::string value)
{
	std::transform(std::begin(value), std::end(value), std::begin(value), [](auto c) { return std::tolower(c); });
	return value;
}

static inline constexpr auto uppercase(std::string value)
{	
	std::transform(std::begin(value), std::end(value), std::begin(value),  [](auto c) { return std::toupper(c); });
	return value;
}


