#pragma once

#include <exception>

template <auto... Args>
struct socket_error_base: std::exception
{
	socket_error_base(std::string_view Message_) noexcept
	:	m_message(Message_)
	{}

	const char* what() const noexcept override
	{
		return m_message.c_str();
	}

private:
	const std::string m_message;
};

using error_socket_timed_out = socket_error_base<1>;
