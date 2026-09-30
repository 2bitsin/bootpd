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
	std::size_t line_no{ 0 };
	while (std::getline(iss, line_s))
	{
		line_no += 1;
		// Skip a UTF-8 byte order mark, as written by some Windows editors.
		if (line_no == 1 && line_s.starts_with("\xEF\xBB\xBF"))
			line_s.erase(0, 3);
		if (line_s.empty())
			continue;
		parse_line(line_s, section_s, line_no);
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

auto config_ini::has_section(std::string_view section) const -> bool
{
	return m_data.contains(std::string(section));
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

auto config_ini::parse_line(std::string_view line_sv, std::string& section, std::size_t line_no) -> config_ini&
{
	using namespace std::string_view_literals;
	using namespace std::string_literals;

	static const auto re_section = std::regex(R"(^\[([^\[\]]*)\]$)", std::regex::optimize);	
	static const auto re_kv_pair = std::regex(R"(^([^=]+)=(.*)$)", std::regex::optimize);

	// Strip a trailing ';' comment, unless the ';' is inside double quotes.
	bool in_quotes = false;
	for (std::size_t pos = 0u; pos < line_sv.size(); ++pos)
	{
		if (line_sv[pos] == '"')
			in_quotes = !in_quotes;
		else if (line_sv[pos] == ';' && !in_quotes) {
			line_sv.remove_suffix(line_sv.size() - pos);
			break;
		}
	}
	trim (line_sv);		
	if (line_sv.empty ())
		return *this;

	std::match_results<std::string_view::iterator> result;
	if (std::regex_match(line_sv.begin(), line_sv.end(), result, re_section))
	{
		std::string_view name_v(result[1].first, result[1].second);
		trim(name_v);
		section = std::string(name_v);
		return *this;		
	}
	if (std::regex_match(line_sv.begin(), line_sv.end(), result, re_kv_pair))
	{
		std::string_view key(result[1].first, result[1].second);
		trim(key);
		if (key.empty())
			throw std::runtime_error(std::format("Missing key name on line {} : {}", line_no, line_sv));
		if (key.size() >= 2u && key.front() == '"' && key.back() == '"')
		{
			key.remove_prefix(1);
			key.remove_suffix(1);
		}

		std::string_view val(result[2].first, result[2].second);
		trim(val);
		if (val.size() >= 2u && val.front() == '"' && val.back() == '"')
		{
			val.remove_prefix(1);
			val.remove_suffix(1);
		}		
		m_data[section][std::string(key)] = std::string(val);
		return *this;
	}
	if (line_no != 0)
	{
		throw std::runtime_error(std::format(
			"Bad line nr {} : {}", line_no, line_sv
		));		
	}
	throw std::runtime_error(std::format(
		"Bad config directive : {}", line_sv
	));
}


auto config_ini::insert_line(std::string_view line_sv, std::string_view section_in) -> config_ini&
{
	std::string section_v (section_in);
	parse_line(line_sv, section_v, 0);	
	return *this;
}