# FREESTANDING guest (wasm32-unknown-unknown) for wasm3 embedding.
# 親プロジェクトのコンパイラ機構を経由せず clang を直接叩く
# (親が gcc でも、CMAKE_CXX_FLAGS に -march=native 等があっても影響を受けない)。
# shim + 汎用ランタイムは freestanding-runtime リポジトリから供給される。

option(YASE_JSON_FREESTANDING "Build wasm32-unknown-unknown freestanding guest" OFF)

if(NOT YASE_JSON_FREESTANDING)
  return()
endif()

find_program(FS_CLANG clang++ REQUIRED)

# freestanding-runtime (shim + 汎用ランタイム)
set(FREESTANDING_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../freestanding-runtime"
  CACHE PATH "freestanding-runtime リポジトリのパス")
if(NOT EXISTS "${FREESTANDING_RUNTIME_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "freestanding-runtime not found at ${FREESTANDING_RUNTIME_DIR}\n"
    "clone it: git clone https://github.com/toge/freestanding-runtime \"${FREESTANDING_RUNTIME_DIR}\"")
endif()
set(FR_BUILD_TESTS OFF)  # yase-json 側では kit 単体テストをビルドしない
add_subdirectory("${FREESTANDING_RUNTIME_DIR}" "${CMAKE_BINARY_DIR}/freestanding-runtime")

# glaze ヘッダーの場所 (vcpkg インストール済みツリーを優先)
find_path(FS_GLAZE_INCLUDE_DIR glaze/glaze.hpp
  HINTS
    "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include"
    "${CMAKE_BINARY_DIR}/../build/vcpkg_installed/x64-linux/include"
)
if(NOT FS_GLAZE_INCLUDE_DIR)
  message(FATAL_ERROR "glaze headers not found — build normally (build.sh) first, then build_wasm.sh")
endif()

# libstdc++ ヘッダーの2ディレクトリ (C++ ヘッダー + arch bits) を検出 (kit のプローブ関数)
freestanding_runtime_find_libstdcxx_dirs(_glibcxx_dir _glibcxx_arch_dir)

set(FS_COMPILE_FLAGS
  "--target=wasm32-unknown-unknown"
  "-nostdlib" "-ffreestanding" "-fno-exceptions" "-fno-rtti"
  "-fno-builtin" "-Oz" "-std=c++23"
  "-D__STDC_HOSTED__=1"
  "-nostdinc++"
  "-isystem${FS_SHIM_EARLY_DIR}"
  "-isystem${_glibcxx_dir}"
  "-isystem${_glibcxx_arch_dir}"
  "-isystem${FS_SHIM_DIR}"
  "-I${CMAKE_SOURCE_DIR}/include"
  "-I${FS_GLAZE_INCLUDE_DIR}"
)
set(FS_LINK_FLAGS
  "--target=wasm32-unknown-unknown"
  "-nostdlib"
  "-Wl,--no-entry"
  "-Wl,-z,stack-size=1048576"
  "-Wl,--export-memory"
  "-Wl,--strip-all"
)

set(FS_GUEST_WASM "${CMAKE_CURRENT_BINARY_DIR}/yase-json-guest.wasm")

add_custom_command(
  OUTPUT "${FS_GUEST_WASM}"
  COMMAND ${FS_CLANG} ${FS_COMPILE_FLAGS} ${FS_LINK_FLAGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/guest.cpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/libstdcxx_support.cpp"
          ${FS_RUNTIME_SOURCES}
          -o "${FS_GUEST_WASM}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/guest.cpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/libstdcxx_support.cpp"
          ${FS_RUNTIME_SOURCES}
          ${FS_SHIM_FILES}
  VERBATIM
)
add_custom_target(yase-json-guest ALL DEPENDS "${FS_GUEST_WASM}")

add_subdirectory(smoke)
