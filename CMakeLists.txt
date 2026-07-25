cmake_minimum_required(VERSION 3.16)
project(scheme-cpp LANGUAGES CXX)

enable_testing()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(scheme STATIC
  src/arity.cpp
  src/builtins.cpp
  src/ctx.cpp
  src/errors.cpp
  src/expression.cpp
  src/lexer.cpp
  src/number.cpp
  src/parser.cpp
  src/preamble.cpp
  src/quasiquote.cpp
  src/reader.cpp
  src/session.cpp
  src/types.cpp
)

target_include_directories(scheme
  PUBLIC include
  PRIVATE src
)
target_compile_features(scheme PRIVATE cxx_std_23)
target_compile_options(scheme PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
set_target_properties(scheme PROPERTIES CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_NO_PACKAGE_REGISTRY ON)
add_subdirectory(third_party/libtommath)
target_link_libraries(scheme PRIVATE libtommath)

if(EMSCRIPTEN)
  add_executable(scheme_wasm web/main.cpp)
  set_target_properties(scheme_wasm PROPERTIES OUTPUT_NAME scheme)
  target_link_libraries(scheme_wasm PRIVATE scheme)

  target_compile_options(scheme PRIVATE -fexceptions)
  target_compile_options(scheme_wasm PRIVATE -fexceptions)
  target_link_options(scheme_wasm PRIVATE
    -lembind
    -sMODULARIZE=1
    -sEXPORT_NAME=createScheme
    -sENVIRONMENT=web,node
    -sALLOW_MEMORY_GROWTH=1
    -sSTACK_SIZE=8MB
    # emscripten 6 defaults to resizable wasm memory, but Firefox's TextDecoder
    # rejects views over resizable ArrayBuffers -> strings >16 bytes crash
    -sGROWABLE_ARRAYBUFFERS=0
    -fexceptions
  )

  add_test(
    NAME scheme-web
    COMMAND node ${CMAKE_CURRENT_SOURCE_DIR}/web/test.cjs
            $<TARGET_FILE:scheme_wasm>
  )
  set_tests_properties(scheme-web PROPERTIES TIMEOUT 30)
else()
  set(MI_BUILD_SHARED OFF)
  set(MI_BUILD_OBJECT OFF)
  set(MI_BUILD_TESTS OFF)
  add_subdirectory(third_party/replxx)
  add_subdirectory(third_party/mimalloc)

  add_executable(scheme_cli cli/main.cpp)
  set_target_properties(scheme_cli PROPERTIES OUTPUT_NAME scheme)
  target_link_libraries(
    scheme_cli
    PRIVATE scheme replxx mimalloc-static
  )

  add_executable(scheme_api_test tests/test_api.cpp)
  target_link_libraries(scheme_api_test PRIVATE scheme)
  add_test(NAME scheme-api COMMAND scheme_api_test)
  add_test(
    NAME scheme-language
    COMMAND scheme_cli tests/all.scm
  )
  set_tests_properties(
    scheme-language
    PROPERTIES
      TIMEOUT 30
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
endif()
