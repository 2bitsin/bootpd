#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <string>

#include <common/socket_api.hpp>
#include <common/byte_order.hpp>
#include <common/serdes.hpp>

#include "dhcp_packet_v4.hpp"

static auto v4_dhcp_opcode_to_string(std::uint8_t opcode)
	-> std::string
{
	using namespace std::string_literals;
	switch (opcode)
	{
	case 1:
		return "Request"s;
	case 2:
		return "Reply"s;
	default:
		return "Other ("s + std::to_string(opcode) + ")"s;
	}
}

static auto v4_dhcp_hardware_type_to_string(std::uint8_t htype)
	-> std::string
{
	using namespace std::string_literals;
	switch (htype)
	{
	case 1:
		return "Ethernet"s;
	default:
		return "Other ("s + std::to_string(htype) + ")"s;
	}
}

// Length of a fixed size, optionally NUL terminated, character field.
template <std::size_t N>
static auto fixed_string_view(const char (&field)[N]) -> std::string_view
{
	return { field, (std::size_t)(std::find(field, field + N, '\0') - field) };
}

static auto v4_dhcp_bootp_flags_to_string(std::uint16_t flags)
	-> std::string
{
	using namespace std::string_literals;
	if (flags & 0x8000u)
		return "Broadcast"s;
	return "Unicast"s;
}


dhcp_packet_v4::dhcp_packet_v4() = default;

dhcp_packet_v4::dhcp_packet_v4(std::span<const std::byte> bits)
{ (::serdes<serdes_reader>(bits))(*this); }

dhcp_packet_v4::dhcp_packet_v4(::serdes<serdes_reader>& _serdes)
{ _serdes(*this); }

dhcp_packet_v4::dhcp_packet_v4(std::vector<std::byte> const& bits)
:	dhcp_packet_v4(std::span<const std::byte>{ bits })
{}

auto dhcp_packet_v4::serdes(::serdes<serdes_writer>& _serdes) const ->::serdes<serdes_writer>&
{
    _serdes(m_opcode);
    _serdes(m_hardware_type);
    _serdes(m_hardware_address_length);
    _serdes(m_number_of_hops);
    _serdes(m_transaction_id);
    _serdes(m_seconds_elapsed);
    _serdes(m_flags);
    _serdes(m_client_ip_address_v4);
    _serdes(m_your_ip_address_v4);
    _serdes(m_server_ip_address_v4);
    _serdes(m_gateway_ip_address_v4);
    _serdes(m_client_hardware_address);
    _serdes(m_server_host_name);
    _serdes(m_boot_file_name);
    _serdes(m_options);
    // Pad to the minimum BOOTP message size.
    while (_serdes.consumed_bytes() < DHCP_MINIMUM_PACKET_SIZE)
        _serdes(DHCP_OPTION_PAD);
    return _serdes;
}

auto dhcp_packet_v4::serdes(::serdes<serdes_reader>& _serdes) ->::serdes<serdes_reader>&
{
	_serdes(m_opcode);
	_serdes(m_hardware_type);
	_serdes(m_hardware_address_length);
	_serdes(m_number_of_hops);
	_serdes(m_transaction_id);
	_serdes(m_seconds_elapsed);
	_serdes(m_flags);
	_serdes(m_client_ip_address_v4);
	_serdes(m_your_ip_address_v4);
	_serdes(m_server_ip_address_v4);
	_serdes(m_gateway_ip_address_v4);
	_serdes(m_client_hardware_address);
	_serdes(m_server_host_name);
	_serdes(m_boot_file_name);
	_serdes(m_options);
	return _serdes;
}

auto dhcp_packet_v4::pretty_print(std::ostream& oss) const ->std::ostream&
{
	using namespace std::string_view_literals;
	oss << std::format("Opcode ................... : {} \n"sv, v4_dhcp_opcode_to_string(m_opcode));
	oss << std::format("Hardware type ............ : {} \n"sv, v4_dhcp_hardware_type_to_string(m_hardware_type));
	oss << std::format("Hardware address length .. : {} \n"sv, m_hardware_address_length);
	oss << std::format("Number of hops ........... : {} \n"sv, m_number_of_hops);
	oss << std::format("Transaction ID ........... : {:#010x} \n"sv, m_transaction_id);
	oss << std::format("Seconds elapsed .......... : {}s \n"sv, m_seconds_elapsed);
	oss << std::format("Bootp flags .............. : {} \n"sv, v4_dhcp_bootp_flags_to_string(m_flags));
	oss << std::format("Client IP address ........ : {} \n"sv, v4_address_to_string(m_client_ip_address_v4));
	oss << std::format("Your IP address .......... : {} \n"sv, v4_address_to_string(m_your_ip_address_v4));
	oss << std::format("Server IP address ........ : {} \n"sv, v4_address_to_string(m_server_ip_address_v4));
	oss << std::format("Gateway IP address ....... : {} \n"sv, v4_address_to_string(m_gateway_ip_address_v4));
	oss << std::format("Client hardware address .. : {} \n"sv, mac_address_to_string(hardware_address()));
	oss << std::format("Server host name ......... : '{}' \n"sv, server_host_name());
	oss << std::format("Boot file name ........... : '{}' \n"sv, boot_file_name());
	
	return oss;
}

auto dhcp_packet_v4::is_message_type(std::uint8_t msg_type) const -> bool
{ auto mto = m_options.message_type(); return mto.has_value() ? *mto == msg_type : false; }

auto dhcp_packet_v4::opcode() const -> std::uint8_t
{
	return m_opcode;
}

auto dhcp_packet_v4::hardware_type() const -> std::uint8_t
{
	return m_hardware_type;
}

auto dhcp_packet_v4::hardware_address_length() const -> std::uint8_t
{
	return m_hardware_address_length;
}

auto dhcp_packet_v4::hardware_address() const -> std::span<const std::uint8_t>
{
	// hlen comes from the wire, never trust it to fit the 16 byte chaddr field.
	const auto length_v = std::min<std::size_t>(m_hardware_address_length, sizeof(m_client_hardware_address));
	return { m_client_hardware_address, length_v };
}

auto dhcp_packet_v4::number_of_hops() const -> std::uint8_t
{
	return m_number_of_hops;
}

auto dhcp_packet_v4::client_address() const -> std::uint32_t
{
	return m_client_ip_address_v4;
}

auto dhcp_packet_v4::your_address() const -> std::uint32_t
{
	return m_your_ip_address_v4;
}

auto dhcp_packet_v4::server_address() const -> std::uint32_t
{
	return m_server_ip_address_v4;
}

auto dhcp_packet_v4::gateway_address() const -> std::uint32_t
{
	return m_gateway_ip_address_v4;
}

auto dhcp_packet_v4::transaction_id() const -> std::uint32_t
{
	return m_transaction_id;
}

auto dhcp_packet_v4::seconds_elapsed() const -> std::uint16_t
{
	return m_seconds_elapsed;
}

auto dhcp_packet_v4::flags() const -> std::uint16_t
{
	return m_flags;
}

auto dhcp_packet_v4::server_host_name() const -> std::string_view
{
	return fixed_string_view(m_server_host_name);
}

auto dhcp_packet_v4::boot_file_name() const -> std::string_view
{
	return fixed_string_view(m_boot_file_name);
}

auto dhcp_packet_v4::message_type() const -> std::optional<std::uint8_t>
{ return m_options.message_type(); }

auto dhcp_packet_v4::message_type(std::uint8_t msg_type) -> dhcp_packet_v4&
{ m_options.message_type(msg_type); return *this; }

auto dhcp_packet_v4::requested_parameters() const -> std::span<const std::uint8_t>
{ return m_options.requested_parameters(); }

auto dhcp_packet_v4::options() -> dhcp_options_v4&
{ return m_options; }

auto dhcp_packet_v4::options() const -> dhcp_options_v4 const&
{ return m_options; }

auto dhcp_packet_v4::assign_options(dhcp_options_v4 const& from, std::span<const std::uint8_t> which) -> dhcp_packet_v4&
{
	for (auto&& index : which)
		m_options.assign(index, from);
	return *this;
}

auto dhcp_packet_v4::assign_options(dhcp_options_v4 const& from, std::initializer_list<std::uint8_t> which) -> dhcp_packet_v4&
{
	return assign_options(from, std::span<const std::uint8_t>(which.begin(), which.size()));
}

auto dhcp_packet_v4::opcode(std::uint8_t value) -> dhcp_packet_v4&
{
	m_opcode = value; return *this;
}

auto dhcp_packet_v4::hardware_type(std::uint8_t value) -> dhcp_packet_v4&
{
	m_hardware_type = value; return *this;
}

auto dhcp_packet_v4::hardware_address(std::span<const std::uint8_t> value) -> dhcp_packet_v4&
{
	if (value.size() > sizeof(m_client_hardware_address))
		throw std::logic_error("hardware_address too large");
	m_hardware_address_length = (std::uint8_t)value.size();
	std::fill(std::begin(m_client_hardware_address), std::end(m_client_hardware_address), 0);
	std::copy(value.begin(), value.end(), m_client_hardware_address);
	return *this;
}

auto dhcp_packet_v4::client_address(std::uint32_t value) -> dhcp_packet_v4&
{
	m_client_ip_address_v4 = value; return *this;
}

auto dhcp_packet_v4::your_address(std::uint32_t value) -> dhcp_packet_v4&
{
	m_your_ip_address_v4 = value; return *this;
}

auto dhcp_packet_v4::server_address(std::uint32_t value) -> dhcp_packet_v4&
{
	m_server_ip_address_v4 = value; return *this;
}

auto dhcp_packet_v4::gateway_address(std::uint32_t value) -> dhcp_packet_v4&
{
	m_gateway_ip_address_v4 = value; return *this;
}

auto dhcp_packet_v4::transaction_id(std::uint32_t value) -> dhcp_packet_v4&
{
	m_transaction_id = value; return *this;
}

auto dhcp_packet_v4::seconds_elapsed(std::uint16_t value) -> dhcp_packet_v4&
{
	m_seconds_elapsed = value; return *this;
}

auto dhcp_packet_v4::flags(std::uint16_t value) -> dhcp_packet_v4&
{
	m_flags = value; return *this;
}

auto dhcp_packet_v4::number_of_hops(std::uint8_t hops) -> dhcp_packet_v4&
{
	m_number_of_hops = hops; return *this;
}

auto dhcp_packet_v4::server_host_name(std::string_view value) -> dhcp_packet_v4&
{
	if (value.size() >= sizeof(m_server_host_name))
		throw std::logic_error("host_name too large");
	std::fill(std::begin(m_server_host_name), std::end(m_server_host_name), 0);
	std::copy(value.begin(), value.end(), m_server_host_name);
	return *this;
}

auto dhcp_packet_v4::boot_file_name(std::string_view value) -> dhcp_packet_v4&
{
	if (value.size() >= sizeof(m_boot_file_name))
		throw std::logic_error("boot_file_name too large");
	std::fill(std::begin(m_boot_file_name), std::end(m_boot_file_name), 0);
	std::copy(value.begin(), value.end(), m_boot_file_name);
	return *this;
}

auto dhcp_packet_v4::serdes_size_hint() const -> std::size_t
{
	return std::max(DHCP_MINIMUM_PACKET_SIZE,
		m_options.serdes_size_hint()
		+ sizeof(m_opcode)
		+ sizeof(m_hardware_type)
		+ sizeof(m_hardware_address_length)
		+ sizeof(m_number_of_hops)
		+ sizeof(m_transaction_id)
		+ sizeof(m_seconds_elapsed)
		+ sizeof(m_flags)
		+ sizeof(m_client_ip_address_v4)
		+ sizeof(m_your_ip_address_v4)
		+ sizeof(m_server_ip_address_v4)
		+ sizeof(m_gateway_ip_address_v4)
		+ sizeof(m_client_hardware_address)
		+ sizeof(m_server_host_name)
		+ sizeof(m_boot_file_name));
}
