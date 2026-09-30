#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <common/serdes.hpp>
#include "dhcp_consts_v4.hpp"

// The DHCP options field (RFC 2132): a magic cookie followed by
// code / length / value triplets, terminated by the END option.
struct dhcp_options_v4
{
	using value_type = std::vector<std::uint8_t>;

	static constexpr auto is_valid_code(std::uint8_t code) noexcept -> bool
	{ return code != DHCP_OPTION_PAD && code != DHCP_OPTION_END; }

	auto serdes(::serdes<serdes_reader>& _serdes)
		-> ::serdes<serdes_reader>&
	{
		m_values = {};
		m_cookie = 0u;

		if (_serdes.remaining_bytes() < sizeof (m_cookie))
			return _serdes;
		_serdes(m_cookie);
		if (m_cookie != DHCP_MAGIC_COOKIE)
			return _serdes;

		// Be lenient with malformed input : a missing END option or a
		// truncated last option ends parsing instead of rejecting the packet.
		while (!_serdes.empty())
		{
			std::uint8_t code{ 0u };
			std::uint8_t size{ 0u };
			_serdes(code);
			if (code == DHCP_OPTION_PAD) continue;
			if (code == DHCP_OPTION_END) break;
			if (_serdes.empty())
				break;
			_serdes(size);
			if (_serdes.remaining_bytes() < size)
				break;

			// Options that appear more than once are concatenated (RFC 3396).
			auto& value_v = m_values[code];
			if (!value_v.has_value())
				value_v.emplace();
			const auto offset = value_v->size();
			value_v->resize(offset + size);
			_serdes(std::span{ value_v->data() + offset, size });
		}
		return _serdes;
	}

	auto serdes(::serdes<serdes_writer>& _serdes) const
		-> ::serdes<serdes_writer>&
	{
		_serdes(std::uint32_t(DHCP_MAGIC_COOKIE));
		for (auto code = 1u; code < m_values.size(); ++code)
		{
			if (!m_values[code].has_value())
				continue;
			_serdes(std::uint8_t(code));
			_serdes(std::uint8_t(m_values[code]->size()));
			_serdes(std::span<const std::uint8_t>{ *m_values[code] });
		}
		_serdes(DHCP_OPTION_END);
		return _serdes;
	}

	auto has(std::uint8_t code) const noexcept -> bool
	{
		return is_valid_code(code) && m_values[code].has_value();
	}

	// Raw option value, empty if the option is not present.
	auto operator[] (std::uint8_t code) const
		-> std::span<const std::uint8_t>
	{
		if (!has(code))
			return {};
		return { *m_values[code] };
	}

	auto erase(std::uint8_t code) -> dhcp_options_v4&
	{
		if (is_valid_code(code))
			m_values[code].reset();
		return *this;
	}

	auto set(std::uint8_t code, std::span<const std::uint8_t> data)
		-> bool
	{
		if (!is_valid_code(code) || data.size() > 255u)
			return false;
		m_values[code].emplace(data.begin(), data.end());
		return true;
	}

	// String valued options (host name, domain name, boot file, ...)
	auto set(std::uint8_t code, std::string_view text)
		-> bool
	{
		return set(code, std::span{ (const std::uint8_t*)text.data(), text.size() });
	}

	// Fixed size values (addresses, times, ...) stored in network byte order.
	template <typename... Q>
	requires (sizeof...(Q) > 0u && ((std::is_arithmetic_v<Q> || std::is_enum_v<Q>) && ...))
	auto set(std::uint8_t code, Q const&... args)
		-> bool
	{
		static constexpr auto total_size = (sizeof(Q) + ... + 0);
		if (!is_valid_code(code) || total_size > 255u)
			return false;

		value_type value_v(total_size);
		::serdes<serdes_writer> _serdes (std::span{ value_v });
		((_serdes(args)), ...);
		m_values[code] = std::move(value_v);
		return true;
	}

	// Decodes an option into the referenced values, false if the option is
	// missing or too short.
	template <typename... Q>
	auto value(std::uint8_t code, std::tuple<Q&...> values) const
		-> bool
	{
		if (!has(code))
			return false;
		try
		{
			::serdes<serdes_reader> _serdes(std::span<const std::uint8_t>{ *m_values[code] });
			std::apply([&_serdes](auto&... value) { ((_serdes(value)), ...); }, values);
			return true;
		}
		catch (std::exception const&)
		{
			return false;
		}
	}

	// Copies option `code` from another set, if present there.
	auto assign(std::uint8_t code, dhcp_options_v4 const& from)
		-> bool
	{
		if (!from.has(code))
			return false;
		m_values[code] = from.m_values[code];
		return true;
	}

	auto message_type() const
		-> std::optional<std::uint8_t>
	{
		std::uint8_t mt_val = 0u;
		if (value(DHCP_OPTION_MESSAGE_TYPE, std::tie(mt_val)))
			return mt_val;
		return std::nullopt;
	}

	auto message_type(std::uint8_t msg_type) -> void
	{
		set(DHCP_OPTION_MESSAGE_TYPE, msg_type);
	}

	auto requested_parameters() const
		-> std::span<const std::uint8_t>
	{
		return (*this)[DHCP_OPTION_PARAMETER_REQUEST_LIST];
	}

	// True if the options area started with the RFC 1048 magic cookie.
	auto has_cookie() const noexcept -> bool
	{
		return m_cookie == DHCP_MAGIC_COOKIE;
	}

	auto serdes_size_hint() const
		-> std::size_t
	{
		std::size_t total_sum = sizeof(m_cookie) + sizeof(std::uint8_t);
		for(auto&& value : m_values)
		{
			if (value.has_value())
				total_sum += value->size() + 2u * sizeof(std::uint8_t);
		}
		return total_sum;
	}

private:
	std::uint32_t m_cookie{ DHCP_MAGIC_COOKIE };
	// Indexed by option code, 0 (PAD) and 255 (END) are never used.
	std::array<std::optional<value_type>, 255u> m_values{};
};
