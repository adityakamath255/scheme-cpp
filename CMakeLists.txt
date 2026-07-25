cmake_minimum_required(VERSION 3.16)
project(scheme-cpp LANGUAGES CXX)

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
  add_subdirectory(web)
else()
  add_subdirectory(cli)

  enable_testing()
  add_executable(scheme_api_test tests/test_api.cpp)
  target_link_libraries(scheme_api_test PRIVATE scheme)
  add_test(NAME scheme-api COMMAND scheme_api_test)
  add_test(
    NAME scheme-language
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh
            $<TARGET_FILE:scheme_cli>
  )
endif()
