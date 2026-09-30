#pragma once

#include "byte_order.hpp"

#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// A tiny symmetric (de)serializer. The same `serdes(...)` member function
// can be written once per packet type and driven by either a reader or a
// writer. Multi-byte scalars are converted to/from network byte order.

enum byte_order_type : bool
{
	network_byte_order = true,
	host_byte_order = false
};

enum serdes_type : bool
{
	serdes_reader,
	serdes_writer
};

struct serdes_asciiz_flag_t {};

static inline constexpr const serdes_asciiz_flag_t serdes_asciiz {};

template <serdes_type Serdes_type, byte_order_type Byte_order = network_byte_order>
struct serdes;

template <byte_order_type Byte_order>
struct serdes<serdes_reader, Byte_order>
{
	static inline const constexpr auto byte_order = Byte_order;
	static inline const constexpr auto serdes_type = serdes_reader;

	template <typename T>
	requires (std::is_trivial_v<T> && sizeof(T) == 1u)
	serdes(std::span<const T> bits)
	:	m_curr{ (const std::byte*)bits.data(), bits.size() },
		m_data{ (const std::byte*)bits.data(), bits.size() }
	{}

	template <typename T>
	requires (std::is_trivial_v<T> && sizeof(T) == 1u && !std::is_const_v<T>)
	serdes(std::span<T> bits)
	: serdes(std::span<const T>(bits))
	{}

	template <typename T>
	requires (std::is_trivial_v<T> && sizeof(T) == 1u)
	serdes(std::vector<T> const& bits)
	: serdes(std::span{ bits })
	{}

	// Reads a NUL terminated string.
	template <typename T>
	requires (std::is_trivial_v<T>)
	auto operator () (std::basic_string<T>& output_value, serdes_asciiz_flag_t, std::string_view = "") -> serdes&
	{
		std::basic_string<T> value;
		while(true)
		{
			T char_v;
			(*this)(char_v);
			if (char_v == T(0))
				break;
			value.push_back(char_v);
		}
		output_value = std::move(value);
		return *this;
	}

	template <typename T>
	requires requires (T& value)
	{ value.serdes(std::declval<serdes&>()); }
	auto operator () (T& value, std::string_view = "") -> serdes&
	{
		value.serdes(*this);
		return *this;
	}

	template <typename T>
	requires (std::is_trivial_v<T>)
	auto operator () (T& value, std::string_view = "") -> serdes&
	{
		if (m_curr.size() < sizeof(T))
			throw std::runtime_error("deserialize: not enough data");
		std::memcpy(&value, m_curr.data(), sizeof(T));
		if constexpr (Byte_order == network_byte_order)
			if constexpr (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
				net_to_host_inplace(value);
		m_curr = m_curr.subspan(sizeof(T));
		return *this;
	}

	template <typename T, std::size_t N>
	auto operator () (T (&output_value)[N], std::string_view = "") -> serdes&
	{
		for (auto& value : output_value)
			(*this)(value);
		return *this;
	}

	template <typename T>
	auto operator () (std::span<T> output_value, std::string_view = "") -> serdes&
	{
		for(auto& value : output_value)
			(*this)(value);
		return *this;
	}

	auto& skip(std::size_t number_of_bytes)
	{
		if (m_curr.size() < number_of_bytes)
			throw std::runtime_error("skipping past end of data");
		m_curr = m_curr.subspan(number_of_bytes);
		return *this;
	}

	auto& seek(std::size_t byte_offset)
	{
		if (m_data.size() < byte_offset)
			throw std::runtime_error("seeking past end of data");
		m_curr = m_data.subspan(byte_offset);
		return *this;
	}

	auto& rewind()
	{
		return seek(0u);
	}

	auto remaining_bytes() const noexcept -> std::size_t
	{
		return m_curr.size();
	}

	auto consumed_bytes() const noexcept -> std::size_t
	{
		return m_data.size() - m_curr.size();
	}

	bool empty() const noexcept
	{
		return m_curr.empty();
	}

private:
	std::span<const std::byte> m_curr;
	std::span<const std::byte> m_data;
};

template <byte_order_type Byte_order>
struct serdes<serdes_writer, Byte_order>
{
	static inline const constexpr auto byte_order = Byte_order;
	static inline const constexpr auto serdes_type = serdes_writer;

	template <typename T>
	requires (std::is_trivial_v<T> && !std::is_const_v<T> && sizeof(T) == 1u)
	serdes(std::span<T> bits)
	:	m_curr{ (std::byte*)bits.data(), bits.size() },
		m_data{ (std::byte*)bits.data(), bits.size() }
	{}

	template <typename T>
	requires (std::is_trivial_v<T> && sizeof(T) == 1u)
	serdes(std::vector<T>& bits)
	: serdes(std::span{ bits })
	{}

	template <typename T>
	requires requires (T const& value)
	{ value.serdes(std::declval<serdes&>()); }
	auto operator () (T const& value, std::string_view = "") -> serdes&
	{
		value.serdes(*this);
		return *this;
	}

	// Writes a NUL terminated string.
	template <typename T>
	requires (std::is_trivial_v<T>)
	auto operator () (std::basic_string<T> const& value, serdes_asciiz_flag_t, std::string_view = "") -> serdes&
	{
		for(auto&& char_v : value)
			(*this)(char_v);
		(*this)(T(0));
		return *this;
	}

	template <typename T>
	requires (std::is_trivial_v<T>)
	auto operator () (T const& value, std::string_view = "") -> serdes&
	{
		if (m_curr.size() < sizeof(T))
			throw std::runtime_error("serialize: not enough space");

		T temp_v = value;
		if constexpr (Byte_order == network_byte_order)
			if constexpr (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
				host_to_net_inplace(temp_v);
		std::memcpy(m_curr.data(), &temp_v, sizeof(T));
		m_curr = m_curr.subspan(sizeof(T));
		return *this;
	}

	template <typename T, std::size_t N>
	auto operator () (const T (&output_value)[N], std::string_view = "") -> serdes&
	{
		for (const auto& value : output_value)
			(*this)(value);
		return *this;
	}

	template <typename T>
	auto operator () (std::span<T> output_value, std::string_view = "") -> serdes&
	{
		for(const auto& value : output_value)
			(*this)(value);
		return *this;
	}

	template <typename T>
	auto operator () (std::basic_string_view<T> output_value, std::string_view = "") -> serdes&
	{
		for(const auto& value : output_value)
			(*this)(value);
		return *this;
	}

	template <typename T>
	auto operator () (std::basic_string<T> const& output_value, std::string_view = "") -> serdes&
	{
		for(const auto& value : output_value)
			(*this)(value);
		return *this;
	}

	auto& skip(std::size_t number_of_bytes)
	{
		if (m_curr.size() < number_of_bytes)
			throw std::runtime_error("skipping past end of data");
		m_curr = m_curr.subspan(number_of_bytes);
		return *this;
	}

	auto& seek(std::size_t byte_offset)
	{
		if (m_data.size() < byte_offset)
			throw std::runtime_error("seeking past end of data");
		m_curr = m_data.subspan(byte_offset);
		return *this;
	}

	auto& rewind()
	{
		return seek(0u);
	}

	auto remaining_bytes() const noexcept -> std::size_t
	{
		return m_curr.size();
	}

	auto consumed_bytes() const noexcept -> std::size_t
	{
		return m_data.size() - m_curr.size();
	}

	bool empty() const noexcept
	{
		return m_curr.empty();
	}

private:
	std::span<std::byte> m_curr;
	std::span<std::byte> m_data;
};

#define SERDES_APPLY(serdes, variable) serdes(variable, #variable)

template <byte_order_type Byte_order = network_byte_order, typename T = void>
requires requires (T const& t) { { t.serdes_size_hint() } -> std::convertible_to<std::size_t>; }
inline auto serialize_to_vector(T const& t) -> std::vector<std::byte>
{
	std::vector<std::byte> temp_v( t.serdes_size_hint() + 128u );
	std::span<std::byte> temp_s{ temp_v };
	serdes<serdes_writer, Byte_order> _serdes(temp_s);
	_serdes(t);
	temp_v.resize(_serdes.consumed_bytes());
	return temp_v;
}
