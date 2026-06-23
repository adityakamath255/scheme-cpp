cmake_minimum_required(VERSION 3.16)
project(scheme-cli LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/core.cmake)

add_executable(scheme main.cpp)
scheme_core(scheme)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/replxx replxx)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/mimalloc mimalloc)
target_link_libraries(scheme PRIVATE replxx mimalloc-static)

set_property(TARGET scheme PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
