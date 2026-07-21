cmake_minimum_required(VERSION 3.16)
project(scheme-wasm LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/core.cmake)

if(NOT TARGET scheme)
  scheme_library(scheme)
endif()

add_executable(scheme_wasm main.cpp)
set_target_properties(scheme_wasm PROPERTIES OUTPUT_NAME scheme)
target_link_libraries(scheme_wasm PRIVATE scheme::scheme)

target_compile_options(scheme PRIVATE -fexceptions)
target_compile_options(scheme_wasm PRIVATE -fexceptions)
target_link_options(scheme_wasm PRIVATE
  -lembind
  -sMODULARIZE=1
  -sEXPORT_NAME=createScheme
  -sENVIRONMENT=web
  -sALLOW_MEMORY_GROWTH=1
  -sSTACK_SIZE=8MB
  # emscripten 6 defaults to resizable wasm memory, but Firefox's TextDecoder
  # rejects views over resizable ArrayBuffers -> strings >16 bytes crash
  -sGROWABLE_ARRAYBUFFERS=0
  -fexceptions)
