#pragma once

#include <functional>
#include <stop_token>

struct control_c
{	
	
	static auto get_token() -> std::stop_token;
	static auto stop_requested() -> bool;
};