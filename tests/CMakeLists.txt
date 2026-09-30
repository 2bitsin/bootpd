find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(bootpd_tests
  test_main.cpp
  test_helpers.hpp
  test_address_v4.cpp
  test_arguments.cpp
  test_async.cpp
  test_config_ini.cpp
  test_dhcp_options.cpp
  test_dhcp_packet.cpp
  test_dhcp_server.cpp
  test_lexical_cast.cpp
  test_serdes.cpp
  test_tftp_packet.cpp
  test_tftp_reader.cpp
  test_tftp_server.cpp
  test_tftp_session.cpp
)
target_link_libraries(bootpd_tests PRIVATE bootpd_core GTest::gtest)

gtest_discover_tests(bootpd_tests DISCOVERY_TIMEOUT 60 PROPERTIES TIMEOUT 60)
