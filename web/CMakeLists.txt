cmake_minimum_required(VERSION 3.16)
project(scheme-wasm LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/core.cmake)

add_executable(scheme main.cpp)
scheme_core(scheme)

target_compile_options(scheme PRIVATE -fexceptions)
target_link_options(scheme PRIVATE
  -lembind
  -sMODULARIZE=1
  -sEXPORT_NAME=createScheme
  -sENVIRONMENT=web
  -sALLOW_MEMORY_GROWTH=1
  -sSTACK_SIZE=8MB
  -fexceptions)
