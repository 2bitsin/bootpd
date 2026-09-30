add_library(common STATIC
  address_v4.hpp
  address_v4.cpp
  arguments.hpp
  arguments.cpp
  async.hpp
  async.cpp
  byte_order.hpp
  config_ini.hpp
  config_ini.cpp
  control_c.hpp
  control_c.cpp
  lexical_cast.hpp
  logger.hpp
  logger.cpp
  serdes.hpp
  socket_api.hpp
  socket_api.cpp
  socket_option.hpp
  socket_option.cpp
  socket_udp.hpp
  socket_udp.cpp
  utility_case.hpp
  utility_trim.hpp
)

# Headers are included as <common/...>
target_include_directories(common PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(common PUBLIC bootpd_build_options)

if(WIN32)
  target_link_libraries(common PUBLIC ws2_32)
  target_compile_definitions(common PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601)
endif()
