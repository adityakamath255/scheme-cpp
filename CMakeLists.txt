cmake_minimum_required(VERSION 3.16)
project(scheme-cpp LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/core.cmake)
scheme_library(scheme)

set(SCHEME_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(SCHEME_TOP_LEVEL ON)
endif()

option(
  SCHEME_BUILD_CLI
  "Build the Scheme command-line application"
  ${SCHEME_TOP_LEVEL}
)
if(SCHEME_BUILD_CLI)
  add_subdirectory(cli)
endif()

option(SCHEME_BUILD_TESTS "Build the Scheme API test" ${SCHEME_TOP_LEVEL})
if(SCHEME_BUILD_TESTS)
  enable_testing()
  add_executable(scheme_api_test tests/test_api.cpp)
  target_link_libraries(scheme_api_test PRIVATE scheme::scheme)
  add_test(NAME scheme-api COMMAND scheme_api_test)

  if(SCHEME_BUILD_CLI)
    add_test(
      NAME scheme-language
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh
              $<TARGET_FILE:scheme_cli>
    )
  endif()
endif()
