cmake_minimum_required(VERSION 3.16)
project(scheme-wasm LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp)
list(REMOVE_ITEM CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/main.cpp)

add_executable(scheme ${CORE_SOURCES} main.cpp)
target_include_directories(scheme PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_compile_options(scheme PRIVATE -Wall -Wextra -fexceptions)
target_link_options(scheme PRIVATE
  -lembind
  -sMODULARIZE=1
  -sEXPORT_NAME=createScheme
  -sENVIRONMENT=web
  -sALLOW_MEMORY_GROWTH=1
  -sSTACK_SIZE=8MB
  -fexceptions)
