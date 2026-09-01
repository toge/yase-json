# wasm3 スモークテスト — WASM3_SOURCE_DIR が利用可能な場合のみビルド・実行する

if(WASM3_SOURCE_DIR AND EXISTS "${WASM3_SOURCE_DIR}/source/wasm3.h")
  enable_language(C)
  file(GLOB WASM3_SOURCES "${WASM3_SOURCE_DIR}/source/*.c")

  add_executable(wasm3_smoke wasm3_smoke.cpp ${WASM3_SOURCES})
  target_include_directories(wasm3_smoke PRIVATE "${WASM3_SOURCE_DIR}/source")
  target_compile_definitions(wasm3_smoke PRIVATE GUEST_WASM="${FS_GUEST_WASM}")
  add_dependencies(wasm3_smoke yase-json-guest)

  add_custom_command(TARGET wasm3_smoke POST_BUILD
    COMMAND wasm3_smoke "${FS_GUEST_WASM}"
    COMMENT "wasm3 smoke test (guest roundtrip)"
    VERBATIM
  )
else()
  message(STATUS "wasm3 not found — smoke test skipped (set WASM3_SOURCE_DIR to enable)")
endif()
