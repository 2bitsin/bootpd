#pragma once

#include <algorithm>
#include <cctype>
#include <string>

inline auto lowercase(std::string value) -> std::string
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return value;
}

inline auto uppercase(std::string value) -> std::string
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return (char)std::toupper(c); });
	return value;
}
