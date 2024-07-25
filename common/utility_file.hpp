#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <fstream>

template <typename T = std::uint8_t>
auto load_file(std::filesystem::filename filename)
	-> std::vector<T>
{
	using namespace std::string_literals;	
	if (!std::filesystem::exists (filename))
		throw std::runtime_error ("file does not exist: "s + filename.string ());
	if (std::filesystem::is_directory (filename))
		throw std::runtime_error ("path is a directory: "s + filename.string ());
	auto size = std::filesystem::file_size (filename);
	if (size == 0)
		return {};
	auto data = std::vector<T> ((size + sizeof(T) - 1u) / sizeof(T), 0);
	auto file = std::ifstream (filename, std::ios::binary);
	file.read((char*)data.data(), size);
	return data;
}

template <typename T>
void save_file(std::filesystem::filename filename, std::span<T> what)
{	
	using namespace std::string_literals;	
	auto file = std::ofstream (filename, std::ios::binary);
	if (!file.good ())
		throw std::runtime_error ("could not open file for writing: "s + filename.string ());
	file.write((char*)what.data(), what.size() * sizeof(T));
}
