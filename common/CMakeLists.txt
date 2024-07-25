cmake_minimum_required(VERSION 3.18)
project(common_library)
add_library(common STATIC 
  arguments.hpp
	arguments.cpp 
	utility_crc32.hpp
	utility_crc32.cpp	
	config_ini.hpp
	config_ini.cpp
	control_c.hpp
  control_c.cpp
	logger.hpp
  logger.cpp
	socket_option.cpp
	socket_option.hpp
	socket_api.hpp
	socket_api.cpp
	socket_error.hpp	
	socket_udp.cpp
	socket_udp.hpp
	address_v4.hpp
	address_v4.cpp
)
set_property(TARGET common PROPERTY CXX_STANDARD 23)

