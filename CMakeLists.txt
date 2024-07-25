cmake_minimum_required(VERSION 3.18)
project(bootpd)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(common)
add_subdirectory(bootpd)
install(TARGETS bootpd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(FILES example/config.ini example/bootpd.bat DESTINATION ${CMAKE_INSTALL_PREFIX})

