#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace details
{
	inline void reverse_bytes_inplace(void* bytes, std::size_t len)
	{
		std::reverse((std::byte*)(bytes), (std::byte*)(bytes) + len);
	}

	template <typename T>
	requires (std::is_trivially_copyable_v<T>)
	inline void swap_if_little_endian(T& value)
	{
		// Network byte order is big endian, nothing to do on big endian hosts.
		if constexpr (std::endian::native == std::endian::little && sizeof(T) > 1u)
			reverse_bytes_inplace(&value, sizeof(value));
	}
}

template <typename T>
requires (std::is_integral_v<T>)
inline auto host_to_net(T value) -> T
{
	details::swap_if_little_endian(value);
	return value;
}

template <typename T>
requires (std::is_integral_v<T>)
inline auto net_to_host(T value) -> T
{
	details::swap_if_little_endian(value);
	return value;
}

template <typename T>
requires (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
inline auto host_to_net_inplace(T& value) -> void
{
	details::swap_if_little_endian(value);
}

template <typename T>
requires (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
inline auto net_to_host_inplace(T& value) -> void
{
	details::swap_if_little_endian(value);
}
