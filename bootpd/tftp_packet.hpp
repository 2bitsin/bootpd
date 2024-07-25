#pragma once

#include <variant>
#include <unordered_map>
#include <format>
#include <string>
#include <string_view>

#include <common/serdes.hpp>

struct tftp_packet
{
	enum error_category_type : std::uint16_t
	{
		undefined,
		file_not_found,
		access_violation,
		disk_full,
		illegal_operation,
		unknown_transfer_id,
		file_already_exists,
		no_such_user		
	};

	static auto error_code_to_string(error_category_type value) noexcept -> std::string; 

	using dictionary_type = std::unordered_map<std::string, std::string>;

	struct type_rrq
	{
		std::string filename;
		std::string xfermode;	
		dictionary_type options;		
	};	

	struct type_wrq
	{
		std::string filename;
		std::string xfermode;	
		dictionary_type options;		
	};

	struct type_data
	{
		std::uint16_t block_id;
		std::vector<std::byte> data;
	};

	struct type_ack
	{ std::uint16_t block_id; };

	struct type_error
	{
		error_category_type error_code;
		std::string error_string;
	};

	struct type_oack
	{
		 dictionary_type options;
	};

	using payload_type = std::variant<std::monostate, type_rrq, type_wrq, type_data, type_ack, type_error, type_oack>;

	tftp_packet();
	tftp_packet(::serdes<serdes_reader>& _serdes);
	tftp_packet(std::span<const std::byte> bits);
	tftp_packet(std::vector<std::byte> const& bits);

	auto serdes(::serdes<serdes_reader>& _serdes) -> ::serdes<serdes_reader>&;	
	auto serdes(::serdes<serdes_writer>& _serdes) const -> ::serdes<serdes_writer>&;	

	auto to_string() const -> std::string;

	template <typename Func>
	auto visit(Func&& func) const
	{ return std::visit(std::forward<Func>(func), m_value); }

	template <typename Func>
	auto visit(Func&& func)
	{ return std::visit(std::forward<Func>(func), m_value); }

	auto opcode() const noexcept -> std::uint16_t;

	auto clear() -> tftp_packet&;
	auto set_rrq(std::string_view filename, std::string_view xfermode, dictionary_type options) -> tftp_packet&;
	auto set_wrq(std::string_view filename, std::string_view xfermode, dictionary_type options) -> tftp_packet&;
	auto set_data(std::uint16_t block_id, std::span<const std::byte> data) -> tftp_packet&;
	auto set_ack(std::uint16_t block_id) -> tftp_packet&;
	auto set_error(error_category_type error_code, std::string_view error_string) -> tftp_packet&;
  auto set_error(error_category_type error_code) -> tftp_packet&;
	auto set_oack(dictionary_type options) -> tftp_packet&;

	static auto make_rrq(std::string_view filename, std::string_view xfermode, dictionary_type options)->tftp_packet;
	static auto make_wrq(std::string_view filename, std::string_view xfermode, dictionary_type options)->tftp_packet;
	static auto make_data(std::uint16_t block_id, std::span<const std::byte> data)->tftp_packet;
	static auto make_ack(std::uint16_t block_id)->tftp_packet;
	static auto make_error(error_category_type error_code, std::string_view error_string)->tftp_packet;
	static auto make_error(error_category_type error_code)->tftp_packet;
	static auto make_oack(dictionary_type options)->tftp_packet;

	auto serdes_size_hint() const noexcept -> std::size_t;	

	template <typename T>
	auto is() const -> bool
	{
		return std::holds_alternative<T>(m_value);
	}

	template <typename T>
	auto as() const -> T const&
	{
		return std::get<T>(m_value);
	}

private:
	payload_type m_value;
};