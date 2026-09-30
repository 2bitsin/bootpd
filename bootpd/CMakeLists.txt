# Everything but main() lives in a library, so the tests can link against it.
add_library(bootpd_core STATIC
  dhcp_consts_v4.hpp
  dhcp_options_v4.hpp
  dhcp_packet_v4.hpp
  dhcp_packet_v4.cpp
  dhcp_server_v4.hpp
  dhcp_server_v4.cpp
  tftp_consts.hpp
  tftp_packet.hpp
  tftp_packet.cpp
  tftp_reader.hpp
  tftp_reader.cpp
  tftp_server_v4.hpp
  tftp_server_v4.cpp
  tftp_session_v4.hpp
  tftp_session_v4.cpp
)
target_include_directories(bootpd_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bootpd_core PUBLIC common)

add_executable(bootpd main.cpp)
target_link_libraries(bootpd PRIVATE bootpd_core)
target_compile_definitions(bootpd PRIVATE BOOTPD_VERSION="${PROJECT_VERSION}")
