#pragma once

#include <cstdint>
#include <string_view>


constexpr uint64_t cxhash(const char* str, std::size_t len)
{
	auto prime = 14695981039346656037ull;
	auto value = 0ull;
	for (auto i = 0u; i < len; ++i) {
		value *= prime;
		value ^= str[i];
	}
	return value;
}

constexpr uint64_t cxhash(const std::string_view str)
{
	return cxhash(str.data(), str.size());
}

constexpr uint64_t operator "" _cxhash (const char* what, std::size_t len)
{
	return cxhash(what, len);
}