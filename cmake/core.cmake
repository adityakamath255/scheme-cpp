# Shared description of the interpreter core. Front-ends include() this and
# call scheme_core(<their executable target>). Self-locating, so it doesn't
# care where the including project sits.

set(SCHEME_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
file(GLOB_RECURSE SCHEME_CORE_SOURCES CONFIGURE_DEPENDS ${SCHEME_ROOT}/src/*.cpp)

function(scheme_core target)
  target_sources(${target} PRIVATE ${SCHEME_CORE_SOURCES})
  target_include_directories(${target} PRIVATE ${SCHEME_ROOT}/include)
  target_compile_features(${target} PRIVATE cxx_std_20)
  target_compile_options(${target} PRIVATE -Wall -Wextra)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
endfunction()
