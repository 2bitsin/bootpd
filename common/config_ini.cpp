#include "config_ini.hpp"
#include "utility_trim.hpp"
#include <string>
#include <string_view>
#include <regex>
#include <format>

config_ini::config_ini(std::istream& iss)
{
	parse(iss);
}

config_ini::config_ini(std::istream&& iss)
:	config_ini(iss)
{}

config_ini::config_ini()
{}

auto config_ini::parse(std::istream& iss)
	-> config_ini&
{
	std::string line_s;
	std::string section_s;
	while (std::getline(iss, line_s))
	{		
		if (line_s.empty())
			continue;
		parse_line(line_s, section_s);
	}
	return *this;
}

auto config_ini::operator[](accessor_type index) const -> std::optional<std::string_view>
{
	return value(index);
}

auto config_ini::sections() const -> std::vector<std::string_view>
{
	std::vector<std::string_view> result;
	for(auto&& [section_name, _] : m_data)
		result.emplace_back(section_name);
	return result;
}

auto config_ini::keynames(std::string_view section) const -> std::vector<std::string_view>
{
	if (auto section_it = m_data.find(std::string(section)); section_it != m_data.end())
	{
		std::vector<std::string_view> result;
		for(auto&& [key, _] : section_it->second)
			result.emplace_back(key);
		return result;
	}	
	return {};
}

auto config_ini::value(accessor_type index) const -> std::optional<std::string_view>
{	
	if (auto section_it = m_data.find(std::string(index.section)); section_it != m_data.end())
	{
		const auto& sect_data = section_it->second;
		if (auto value_it = sect_data.find(std::string(index.keyname)); value_it != sect_data.end())
			return (*value_it).second;
	}
	return std::nullopt;
}

auto config_ini::parse_line(std::string_view line_sv, std::string& section) -> config_ini&
{
	using namespace std::string_view_literals;
	using namespace std::string_literals;

	static const auto re_section = std::regex(R"(^\[([^\[\]]*)\]$)", std::regex::optimize);	
	static const auto re_kv_pair = std::regex(R"(^([^=]+)=(.*)$)", std::regex::optimize);

	std::string_view::size_type pos;	
	pos = line_sv.find('#');	
	if (pos != line_sv.npos)
		line_sv.remove_suffix(line_sv.size() - pos);
	trim (line_sv);		
	if (line_sv.empty ())
		return *this;

	std::match_results<std::string_view::iterator> result;
	if (std::regex_match(line_sv.begin(), line_sv.end(), result, re_section))
	{
		section = result[1].str();
		return *this;		
	}
	if (std::regex_match(line_sv.begin(), line_sv.end(), result, re_kv_pair))
	{
		std::string_view key(result[1].first, result[1].second);
		trim(key);
		if (key.front() == '"' && key.back() == '"') 
		{
			key.remove_prefix(1);
			key.remove_suffix(1);
		}

		std::string_view val(result[2].first, result[2].second);
		trim(val);
		if (val.front() == '"' && val.back() == '"') 
		{
			val.remove_prefix(1);
			val.remove_suffix(1);
		}		
		m_data[section][std::string(key)] = std::string(val);
		return *this;
	}
	throw std::runtime_error("Bad line : "s + std::string(line_sv));		
}


auto config_ini::insert_line(std::string_view line_sv, std::string_view section_in) -> config_ini&
{
	std::string section_v (section_in);
	parse_line(line_sv, section_v);	
	return *this;
}