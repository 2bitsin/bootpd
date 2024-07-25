#pragma once

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <optional>

#include <common/serdes.hpp>

#include "dhcp_consts_v4.hpp"
#include "dhcp_options_v4.hpp"

struct dhcp_packet_v4
{
	using mac_address_type = std::uint8_t[6];	
	dhcp_packet_v4();
	dhcp_packet_v4(std::span<const std::byte> bits);	
	dhcp_packet_v4(::serdes<serdes_reader>& _serdes);
	dhcp_packet_v4(std::vector<std::byte> const& bits);
	
	auto serdes(::serdes<serdes_writer>& _serdes) const -> ::serdes<serdes_writer>&;		
	auto serdes(::serdes<serdes_reader>& _serdes) -> ::serdes<serdes_reader>&;
	auto pretty_print(std::ostream& oss) const -> std::ostream&;	
	
	auto options()->dhcp_options_v4&;
	auto options() const->dhcp_options_v4 const&;	
	auto assign_options(dhcp_options_v4 const& from, std::span<const std::uint8_t> which) -> dhcp_packet_v4&;
	auto assign_options(dhcp_options_v4 const& from, std::initializer_list<const std::uint8_t> which) -> dhcp_packet_v4&;
	
	auto requested_parameters() const ->std::span<const std::uint8_t>;
	auto message_type(std::uint8_t msg_type) -> dhcp_packet_v4&;	
	auto message_type() const -> std::optional<std::uint8_t>;
	auto is_message_type(std::uint8_t msg_type) const -> bool;

	auto opcode() const->std::uint8_t;
	auto hardware_type() const->std::uint8_t;
	auto hardware_address_length() const->std::uint8_t;
	auto hardware_address() const->std::span<const std::uint8_t>;
	auto number_of_hops() const->std::uint8_t;
	auto client_address() const->std::uint32_t;
	auto your_address() const->std::uint32_t;
	auto server_address() const->std::uint32_t;
	auto gateway_address() const->std::uint32_t;
	auto transaction_id() const->std::uint32_t;
	auto seconds_elapsed() const->std::uint16_t;
	auto flags() const->std::uint16_t;
	auto server_host_name() const->std::string_view;
	auto boot_file_name() const->std::string_view;

	auto opcode(std::uint8_t value)->dhcp_packet_v4&;
	auto hardware_type(std::uint8_t value)->dhcp_packet_v4&;
	auto hardware_address(std::span<const std::uint8_t> value)->dhcp_packet_v4&;
	auto client_address(std::uint32_t value)->dhcp_packet_v4&;
	auto your_address(std::uint32_t value)->dhcp_packet_v4&;
	auto server_address(std::uint32_t value)->dhcp_packet_v4&;
	auto gateway_address(std::uint32_t value)->dhcp_packet_v4&;
	auto transaction_id(std::uint32_t value)->dhcp_packet_v4&;
	auto seconds_elapsed(std::uint16_t value)->dhcp_packet_v4&;
	auto flags(std::uint16_t value)->dhcp_packet_v4&;
	auto number_of_hops(std::uint8_t hops)->dhcp_packet_v4&;
	auto server_host_name(std::string_view value)->dhcp_packet_v4&;
	auto boot_file_name(std::string_view value)->dhcp_packet_v4&;
	
	auto serdes_size_hint() const->std::size_t;

	friend inline auto operator<<(std::ostream& oss, dhcp_packet_v4 const& pkt) -> std::ostream&
	{
		return pkt.pretty_print(oss);
	}

protected:

	// Primary
	std::uint8_t		m_opcode { DHCP_OPCODE_RESPONSE };
	std::uint8_t		m_hardware_type { DHCP_HARDWARE_TYPE_ETHERNET };
	std::uint8_t		m_hardware_address_length { sizeof(mac_address_type) };
	std::uint8_t		m_number_of_hops { 0 };
	std::uint32_t		m_transaction_id { 0 };
	std::uint16_t		m_seconds_elapsed { 0 };
	std::uint16_t		m_flags { DHCP_FLAGS_BROADCAST };
	std::uint32_t		m_client_ip_address_v4  { 0 };
	std::uint32_t		m_your_ip_address_v4 { 0 };
	std::uint32_t		m_server_ip_address_v4 { 0 };
	std::uint32_t		m_gateway_ip_address_v4 { 0 };
	std::uint8_t		m_client_hardware_address [16] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	char						m_server_host_name [64] { 0 };
	char						m_boot_file_name [128] { 0 };
	dhcp_options_v4 m_options;
};


