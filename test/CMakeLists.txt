# Detect WASI target
if(CMAKE_SYSTEM_NAME STREQUAL "WASI" OR CMAKE_SYSTEM_NAME STREQUAL "Wasi" OR DEFINED(__wasi__))
    # WASI target: build mock tests without Catch2
    # Catch2 depends on signal.h which is not available on wasm32-wasip1
    add_executable(mock_test_wasi mock_test_wasi.cpp)
    target_link_libraries(mock_test_wasi PRIVATE yase-json)
    target_compile_features(mock_test_wasi PUBLIC cxx_std_23)
    # Emscripten は -sDISABLE_EXCEPTION_CATCHING=0 で例外を有効化するため -fno-exceptions と衝突する
    target_compile_options(mock_test_wasi PRIVATE
        $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<NOT:$<BOOL:${EMSCRIPTEN}>>>:-fno-exceptions>)
    add_test(NAME mock_test_wasi COMMAND mock_test_wasi)
else()
    # Hosted target: build Catch2 tests
    find_package(Catch2 REQUIRED CONFIG)

    file(GLOB test_src test_*.cpp)
    # mock_test_wasi is already built as separate target, exclude from Catch2 build
    list(FILTER test_src EXCLUDE REGEX "mock_test_wasi\\.cpp$")

    add_executable(all_test ${test_src})

    target_link_libraries(all_test PRIVATE Catch2::Catch2 Catch2::Catch2WithMain yase-json)
    target_include_directories(all_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
    target_compile_features(all_test PRIVATE cxx_std_23)

    add_test(NAME all_test COMMAND all_test -r junit)
endif()
