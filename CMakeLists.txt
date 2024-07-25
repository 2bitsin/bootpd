cmake_minimum_required(VERSION 3.18)
project(tools)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(common)
add_subdirectory(calculator)
add_subdirectory(serbootd)
add_subdirectory(bootpd)
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/bin)
install(TARGETS calculator serbootd bootpd DESTINATION ${CMAKE_INSTALL_PREFIX})


