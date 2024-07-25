#pragma once

#include <algorithm>
#include <cstddef>

#ifdef _MSC_VER
	#include <intrin.h>
#endif

namespace details
{
	inline void reverse_bytes_inplace(void* bytes, std::size_t len)
	{
		std::reverse((std::byte*)(bytes), (std::byte*)(bytes) + len);
	}

	template <typename T>
	requires(std::is_trivial_v<T>)
	inline void reverse_bytes_inplace(T& value)
	{
		if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_enum_v<T>)
		{
		#ifdef _MSC_VER

			if constexpr (sizeof(T) == sizeof(std::uint16_t))
				value = (T)_byteswap_ushort((std::uint16_t const&)value);
			else if constexpr (sizeof(T) == sizeof(std::uint32_t))
				value = (T)_byteswap_ulong((std::uint32_t const&)value);
			else if constexpr (sizeof(T) == sizeof(std::uint64_t))
				value = (T)_byteswap_uint64((std::uint64_t const&)value);
			else 
				reverse_bytes_inplace(&value, sizeof(value));
			
		#elif defined(__GNUC__) || defined(__clang__)

			if constexpr (sizeof(T) == sizeof(std::uint16_t))
				value = (T)__builtin_bswap16((std::uint16_t const&)value);
			else if constexpr (sizeof(T) == sizeof(std::uint32_t))
				value = (T)__builtin_bswap32((std::uint32_t const&)value);
			else if constexpr (sizeof(T) == sizeof(std::uint64_t))
				value = (T)__builtin_bswap64((std::uint64_t const&)value);
			else 
				reverse_bytes_inplace(&value, sizeof(value));
			
		#else

			if constexpr (sizeof (T) > 1u) 
			{
				reverse_bytes_inplace(&value, sizeof(value));
			}
			
		#endif
		}
		else
		{
			if constexpr (sizeof (T) > 1u) 
			{
				reverse_bytes_inplace(&value, sizeof(value));
			}
		}
	}
}

template <typename T>
requires (std::is_integral_v<T>)
inline auto host_to_net(T value) -> T
{
	details::reverse_bytes_inplace(value);
	return value;
}

template <typename T>
requires (std::is_integral_v<T>)
inline auto net_to_host(T value) -> T
{
	details::reverse_bytes_inplace(value);
	return value;
}

template <typename T>
requires (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
inline auto host_to_net_inplace(T& value) -> void
{
	details::reverse_bytes_inplace(value);
}

template <typename T>
requires (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
inline auto net_to_host_inplace(T& value) -> void
{
	details::reverse_bytes_inplace(value);
}

