# WASI Minimal 向けスモークは常時ビルド可能（hosted 依存なし）
add_executable(smoke_wasi_minimal smoke_wasi_minimal.cpp)
target_link_libraries(smoke_wasi_minimal PRIVATE yase-json)
target_compile_features(smoke_wasi_minimal PUBLIC cxx_std_23)
target_compile_definitions(smoke_wasi_minimal PRIVATE YASE_JSON_WASI_MINIMAL=1)
# Emscripten は -sDISABLE_EXCEPTION_CATCHING=0 で例外を有効化するため -fno-exceptions と衝突する
target_compile_options(smoke_wasi_minimal PRIVATE $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<NOT:$<BOOL:${EMSCRIPTEN}>>>:-fno-exceptions>)
add_test(NAME smoke_wasi_minimal COMMAND smoke_wasi_minimal)

if(ENABLE_WASI_MINIMAL)
  # Catch2 は signal.h に依存し wasm32-wasip1 でビルド不可のため hosted テストをスキップ
  # （vcpkg.json で catch2 は emscripten | !wasm32 に限定）
  return()
endif()

find_package(Catch2 REQUIRED CONFIG)

file(GLOB test_src test_*.cpp)
# smoke_wasi_minimal は既に別ターゲットとしてビルド済みのため除外
list(FILTER test_src EXCLUDE REGEX "smoke_wasi_minimal\\.cpp$")

add_executable(all_test ${test_src})

target_link_libraries(all_test PRIVATE Catch2::Catch2 Catch2::Catch2WithMain yase-json)
target_include_directories(all_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
target_compile_features(all_test PRIVATE cxx_std_23)

add_test(NAME all_test COMMAND all_test -r junit)
