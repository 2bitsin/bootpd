#include "control_c.hpp"
#include "logger.hpp"

#include <mutex>
#include <thread>
#include <iostream>

#include <Windows.h>

static std::stop_source G_source;
static std::once_flag G_initialize;

static auto control_c_handler(DWORD w) -> BOOL
{
	if (w == CTRL_C_EVENT) 
	{
		Glog.debug("Control+C termination requested.");
		G_source.request_stop();
		return TRUE;
	}
	return FALSE;
}

auto control_c::get_token() -> std::stop_token
{
	std::call_once(G_initialize, []() 
	{			
		if(!SetConsoleCtrlHandler(control_c_handler, TRUE)) 
		{
			throw std::runtime_error("Unable to install control+c handler.");
		}		
		std::atexit([] () 
		{
			SetConsoleCtrlHandler(control_c_handler, FALSE);
		});		
	});
	return G_source.get_token();
}

auto control_c::stop_requested() -> bool
{
	static auto token = get_token();
  return token.stop_requested();
}
