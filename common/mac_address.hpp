#pragma once

struct mac_address
{
	mac_address(std::string_view init);
		
private:
	std::uint8_t m_bytes[6];
};