cmake_minimum_required(VERSION 3.16)
project(scheme-cpp LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_executable(scheme ${SOURCES})

target_include_directories(scheme PRIVATE include)
target_compile_options(scheme PRIVATE -Wall -Wextra)

add_subdirectory(third_party/replxx)
add_subdirectory(third_party/mimalloc)
target_link_libraries(scheme PRIVATE replxx mimalloc-static)
