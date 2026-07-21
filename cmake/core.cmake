set(SCHEME_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

set(SCHEME_CORE_SOURCES
  ${SCHEME_ROOT}/src/builtins.cpp
  ${SCHEME_ROOT}/src/eval.cpp
  ${SCHEME_ROOT}/src/expression.cpp
  ${SCHEME_ROOT}/src/lexer.cpp
  ${SCHEME_ROOT}/src/number.cpp
  ${SCHEME_ROOT}/src/parser.cpp
  ${SCHEME_ROOT}/src/preamble.cpp
  ${SCHEME_ROOT}/src/quasiquote.cpp
  ${SCHEME_ROOT}/src/reader.cpp
  ${SCHEME_ROOT}/src/session.cpp
  ${SCHEME_ROOT}/src/types.cpp
)

set(SCHEME_PRIVATE_HEADERS
  ${SCHEME_ROOT}/src/builtins.hpp
  ${SCHEME_ROOT}/src/eval.hpp
  ${SCHEME_ROOT}/src/expression.hpp
  ${SCHEME_ROOT}/src/lexer.hpp
  ${SCHEME_ROOT}/src/number.hpp
  ${SCHEME_ROOT}/src/preamble.hpp
  ${SCHEME_ROOT}/src/quasiquote.hpp
  ${SCHEME_ROOT}/src/reader.hpp
  ${SCHEME_ROOT}/src/types.hpp
)

function(scheme_library target)
  add_library(${target} STATIC
    ${SCHEME_CORE_SOURCES}
    ${SCHEME_PRIVATE_HEADERS}
    ${SCHEME_ROOT}/include/scheme/session.hpp
  )
  add_library(scheme::scheme ALIAS ${target})

  target_include_directories(${target}
    PUBLIC
      $<BUILD_INTERFACE:${SCHEME_ROOT}/include>
      $<INSTALL_INTERFACE:include>
    PRIVATE ${SCHEME_ROOT}/src
  )
  target_compile_features(${target} PRIVATE cxx_std_23)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
  )
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

  if(NOT TARGET libtommath)
    set(CMAKE_EXPORT_NO_PACKAGE_REGISTRY ON)
    add_subdirectory(
      ${SCHEME_ROOT}/third_party/libtommath
      ${CMAKE_BINARY_DIR}/libtommath
    )
  endif()
  target_link_libraries(${target} PRIVATE libtommath)
endfunction()
