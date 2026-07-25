add_executable(scheme_wasm main.cpp)
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
  COMMAND node ${CMAKE_CURRENT_SOURCE_DIR}/test.cjs $<TARGET_FILE:scheme_wasm>
)
set_tests_properties(scheme-web PROPERTIES TIMEOUT 30)
