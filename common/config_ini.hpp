#pragma once

#include <iostream>
#include <unordered_map>
#include <string>
#include <string_view>
#include <optional>
#include <stdexcept>
#include <vector>

#include "lexical_cast.hpp"

struct config_ini
{
	struct accessor_type
	{
		accessor_type(std::string_view keyname, std::string_view section): 
			keyname(keyname),
			section(section) 
		{}
		
		accessor_type(std::string_view keyname): 
			accessor_type(keyname, "")
		{}

		const std::string_view keyname;
		const std::string_view section;
	};
	
	struct section_type
	{
		section_type(std::string_view section): 
			section(section) 
		{}		
		
		const std::string_view section;

		auto operator [] (std::string_view keyname) -> accessor_type 
		{ return accessor_type(keyname, section); }
		
	};	

	config_ini();
	config_ini(std::istream& iss);
	config_ini(std::istream&& iss);

	auto parse(std::istream& iss) -> config_ini&;
		
	auto operator [](accessor_type index) const -> std::optional<std::string_view>;

	auto has_section(std::string_view section) const -> bool;
	auto sections() const -> std::vector<std::string_view>;
	auto keynames(std::string_view section = "") const -> std::vector<std::string_view>;		
	auto value(accessor_type index) const -> std::optional<std::string_view>;	

	// Returns the value converted to T, std::nullopt if the key is missing.
	// Throws std::runtime_error if the value exists but can't be converted.
	template <typename T>
	auto value_as(accessor_type index) const -> std::optional<T>
	{
		const auto optional_value = value (index);
		if (!optional_value.has_value())
			return std::nullopt;
		try
		{
			return lexical_cast<T>(optional_value.value());
		}
		catch(std::exception const& ex)
		{
			std::string key_name (index.keyname);
			if (!index.section.empty())
				key_name = "[" + std::string(index.section) + "] " + key_name;
			throw std::runtime_error("Invalid value for '" + key_name + "' : " + ex.what());
		}
	}

	template <typename T>
	auto value_or(accessor_type index, T const& alternative) const -> T
	{
		return value_as<T>(index).value_or(alternative);
	}

	auto insert_line(std::string_view line_sv, std::string_view section = "") -> config_ini&;

protected:
	auto parse_line(std::string_view line_sv, std::string& section_s, std::size_t line_no) -> config_ini&;
	
private:
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_data;
};