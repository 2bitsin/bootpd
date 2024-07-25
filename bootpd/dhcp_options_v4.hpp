#pragma once

#include <array>
#include <memory>
#include <span>
#include <tuple>
#include <algorithm>
#include <utility>

#include <common/serdes.hpp>
#include "dhcp_consts_v4.hpp"

struct dhcp_options_v4
{
	dhcp_options_v4()
	: m_cookie{ DHCP_MAGIC_COOKIE }, 
		m_values{} 
	{}
	
	dhcp_options_v4(dhcp_options_v4 const& p)
	:	dhcp_options_v4()
	{
		for (auto i = 0u; i < m_values.size(); ++i)
		{
			if (!p.m_values[i])
				continue;
			auto size = p.m_values[i][0] + 1u;
			auto data = std::make_unique<std::uint8_t[]>(size);
			std::copy(p.m_values[i].get(), p.m_values[i].get() + size, data.get());
			m_values[i] = std::move(data);
		}
	}

	auto operator = (dhcp_options_v4 const& p) 
		-> dhcp_options_v4&
	{
		this->~dhcp_options_v4();
		new (this) dhcp_options_v4(p);		
		return *this;
	}
	
	dhcp_options_v4(dhcp_options_v4&& prev) noexcept
	:	m_values{ std::move(prev.m_values) }
	{}
	
	auto operator = (dhcp_options_v4&& prev) noexcept -> dhcp_options_v4& 
	{
		auto tmp = std::move(prev);
		std::swap(tmp, *this);
		return *this;
	}

	void swap(dhcp_options_v4& other) noexcept
	{
		std::swap(m_values, other.m_values);
	}

	auto serdes(::serdes<serdes_reader>& _serdes) 
		-> ::serdes<serdes_reader>&
	{
		using std::make_unique;
		using std::unique_ptr;
		using std::uint8_t;
		using std::span;

		unique_ptr<uint8_t[]> data;
		uint8_t code;
		uint8_t size;
		
		if (_serdes.remaining_bytes() < sizeof (m_cookie))
			return _serdes;
		_serdes(m_cookie);
		if (m_cookie != DHCP_MAGIC_COOKIE)
			return _serdes;
		
		while(true)
		{
			_serdes(code);
			if (code == 0x00u) continue;
			if (code == 0xffu) break;
			_serdes(size);
			data = make_unique<uint8_t[]>(size + 1u);
			data[0] = size;
			_serdes(span{ data.get() + 1u, size });
			m_values[code - 1u] = std::move (data);
		}		
		return _serdes;
	}

	auto serdes(::serdes<serdes_writer>& _serdes) const
		-> ::serdes<serdes_writer>&
	{
		using std::make_unique;
		using std::unique_ptr;
		using std::uint8_t;
		using std::span;
		
		_serdes(std::uint32_t(DHCP_MAGIC_COOKIE));
		const auto count_options = std::min<std::size_t>(std::size(m_values), 254u);
		for (auto i = 0u; i < count_options; ++i)
		{
			if (m_values[i] == nullptr)
				continue;
			_serdes(std::uint8_t(i + 1u));
			_serdes(span{ m_values[i].get(), m_values[i].get()[0] + 1u});
		}	
		_serdes(std::uint8_t(0xff));
		return _serdes;
	}
	
	auto operator[] (std::uint8_t index) const
		-> std::span<const std::uint8_t>
	{
		using namespace std::string_literals;
		if (index <= 0x00u || index >= 0xffu)
			throw std::out_of_range("Accessing invalid option: "s + std::to_string(index));
		if (m_values [index - 1u] == nullptr) 
			return {};
		const auto option_bytes = m_values [index - 1u].get();
		return { option_bytes + 1u, option_bytes[0u]};
	}

	auto set(std::uint8_t code, std::span<const std::uint8_t> data)
		-> bool
	{
		using std::make_unique;
		using std::uint8_t;
		if (code < 1u || code > 254u || data.size() > 255u)
			return false;
		m_values[code - 1u] = make_unique<uint8_t[]>(data.size() + 1u);
		m_values[code - 1u][0] = (std::uint8_t)data.size();
		std::copy(data.begin(), data.end(), m_values[code - 1u].get() + 1u);
	}
	
	template <typename... Q>
	auto set(std::uint8_t code, Q&&... args)
		-> bool
	{
		using std::make_unique;
		using std::uint8_t;

		static constexpr auto total_size = (sizeof(Q) + ... + 0);
		if (total_size > 255u)
			return false;
		
		m_values[code - 1u] = make_unique<uint8_t[]>(total_size + 1u);
		m_values[code - 1u][0] = (uint8_t)total_size;		
		::serdes<serdes_writer> _serdes (
			std::span(m_values[code - 1u].get() + 1u, total_size)
		);
		((_serdes(std::forward<Q>(args))), ...);
		return true;
	}

	template <typename... Q>
	auto value(std::uint8_t code, std::tuple<Q...>& values) const
		-> bool
	{
		return value(code, values, std::make_index_sequence<sizeof...(Q)>());	
	}

	template <typename... Q>
	auto value(std::uint8_t code, std::tuple<Q&...> values) const
		-> bool
	{
		return value(code, values, std::make_index_sequence<sizeof...(Q)>());	
	}

	auto assign(std::uint8_t code, dhcp_options_v4 const& from)
	{
		using std::make_unique;
		if (from.m_values[code - 1u] == nullptr)
			return false;
		auto& src_value = from.m_values[code - 1u];
		auto& dst_value = m_values[code - 1u];
		dst_value = make_unique<uint8_t[]>(src_value[0] + 1u);
		std::copy(src_value.get(), src_value.get() + src_value[0] + 1u, dst_value.get());
		return true;
	}
	
	auto message_type() const 
		-> std::optional<std::uint8_t>
	{
		std::uint8_t mt_val = 0u;
		if (value(0x35, std::tie(mt_val)))
			return mt_val;
		return std::nullopt;
	}
	
	auto message_type(std::uint8_t msg_type)
	{
		set(0x35, msg_type);
	}

	auto requested_parameters() const 
		-> std::span<const std::uint8_t>
	{
		return (*this)[0x37];
	}

	auto serdes_size_hint() const 
		-> std::size_t
	{
		std::size_t total_sum = sizeof(std::uint8_t) + sizeof(m_cookie);
		for(auto&& value : m_values)
		{
			if (value == nullptr)
				continue;
			total_sum += (value[0] + 2*sizeof(std::uint8_t));
		}
		return total_sum;
	}
	
protected:
	template <typename Tuple, std::size_t ... Index>
	auto value(std::uint8_t code, Tuple& values, std::index_sequence<Index...>) const
		-> bool
	{
		if (code < 1u || code > 254u || !m_values[code - 1u])
			return false;
		
		::serdes<serdes_reader> _serdes(
			std::span(m_values[code - 1u].get() + 1u, m_values[code - 1u][0])
		);

		((_serdes(std::get<Index>(values))),...);
		return true;
	}

private:
	std::uint32_t m_cookie;
	std::array<std::unique_ptr<std::uint8_t[]>, 254u> m_values;
};