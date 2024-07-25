cmake_minimum_required(VERSION 3.18)
project(bootpd)
add_executable(bootpd 
  main.cpp 
  dhcp_packet_v4.hpp
  dhcp_packet_v4.cpp
  dhcp_server_v4.hpp  
  dhcp_server_v4.cpp
  dhcp_options_v4.hpp 
  tftp_session_v4.hpp 
  tftp_session_v4.cpp
  tftp_server_v4.cpp
  tftp_server_v4.hpp
  tftp_packet.hpp 
  tftp_packet.cpp 
  tftp_reader.hpp
  tftp_reader.cpp
)

target_link_libraries(bootpd PRIVATE common)
set_property(TARGET bootpd PROPERTY CXX_STANDARD 23)


