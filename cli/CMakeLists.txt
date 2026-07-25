set(MI_BUILD_SHARED OFF)
set(MI_BUILD_OBJECT OFF)
set(MI_BUILD_TESTS OFF)
add_subdirectory(../third_party/replxx replxx)
add_subdirectory(../third_party/mimalloc mimalloc)

add_executable(scheme_cli main.cpp)
set_target_properties(scheme_cli PROPERTIES OUTPUT_NAME scheme)
target_link_libraries(
  scheme_cli
  PRIVATE scheme replxx mimalloc-static
)
