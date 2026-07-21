cmake_minimum_required(VERSION 3.16)
project(scheme-cli LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/core.cmake)

if(NOT TARGET scheme)
  scheme_library(scheme)
endif()

add_executable(scheme_cli main.cpp)
set_target_properties(scheme_cli PROPERTIES OUTPUT_NAME scheme)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/replxx replxx)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/mimalloc mimalloc)
target_link_libraries(
  scheme_cli
  PRIVATE scheme::scheme replxx mimalloc-static
)

set_property(
  TARGET scheme scheme_cli
  PROPERTY INTERPROCEDURAL_OPTIMIZATION ON
)
