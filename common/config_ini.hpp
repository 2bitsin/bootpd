#pragma once

#include <iostream>
#include <unordered_map>
#include <any>
#include <string>
#include <string_view>
#include <optional>

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

	auto sections() const -> std::vector<std::string_view>;
	auto keynames(std::string_view section = "") const -> std::vector<std::string_view>;		
	auto value(accessor_type index) const -> std::optional<std::string_view>;	

	template <typename T>
	auto value_as(accessor_type index) const -> std::optional<T>
	{
		try
		{
			if (const auto optional_value = value (index); optional_value.has_value()) 
				return lexical_cast<T>(optional_value.value());
		}
		catch(bad_lexical_cast const& ex)
		{}		
		return std::nullopt; 
	}

	template <typename T>
	auto value_or(accessor_type index, T const& alternative) const -> T
	{
		return value_as<T>(index).value_or(alternative);
	}

	auto insert_line(std::string_view line_sv, std::string_view section = "") -> config_ini&;

protected:
	auto parse_line(std::string_view line_sv, std::string& section_s) -> config_ini&;
	
private:
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_data;
};