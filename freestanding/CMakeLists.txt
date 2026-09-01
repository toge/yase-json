# FREESTANDING guest (wasm32-unknown-unknown) for wasm3 embedding.
# 親プロジェクトのコンパイラ機構を経由せず clang を直接叩く
# (親が gcc でも、CMAKE_CXX_FLAGS に -march=native 等があっても影響を受けない)。

option(YASE_JSON_FREESTANDING "Build wasm32-unknown-unknown freestanding guest" OFF)

if(NOT YASE_JSON_FREESTANDING)
  return()
endif()

find_program(FS_CLANG clang++ REQUIRED)

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

# libstdc++ ヘッダーの2ディレクトリ (C++ ヘッダー + arch bits) を検出
set(_probe_cc "")
if(CMAKE_CXX_COMPILER MATCHES "clang\\+\\+|g\\+\\+|c\\+\\+")
  set(_probe_cc "${CMAKE_CXX_COMPILER}")
else()
  find_program(_probe_cc g++)
endif()
if(NOT _probe_cc)
  find_program(_probe_cc clang++)
endif()
execute_process(
  COMMAND ${_probe_cc} -E -x c++ -v /dev/null
  OUTPUT_QUIET ERROR_VARIABLE _probe_out
  RESULT_VARIABLE _probe_res
)
if(NOT _probe_res EQUAL 0)
  message(FATAL_ERROR "failed to probe C++ include dirs with ${_probe_cc}")
endif()
set(_marker "#include <...> search starts here:")
string(FIND "${_probe_out}" "${_marker}" _pos)
string(FIND "${_probe_out}" "End of search list" _end)
if(_pos EQUAL -1 OR _end EQUAL -1)
  message(FATAL_ERROR "could not parse include dirs from ${_probe_cc}")
endif()
string(LENGTH "${_marker}" _marker_len)
math(EXPR _pos "${_pos} + ${_marker_len} + 1")
math(EXPR _len "${_end} - ${_pos}")
string(SUBSTRING "${_probe_out}" ${_pos} ${_len} _dirs)
string(REPLACE "\n" ";" _dirs "${_dirs}")
set(_clean_dirs "")
foreach(_d IN LISTS _dirs)
  string(STRIP "${_d}" _d)
  if(NOT _d STREQUAL "")
    list(APPEND _clean_dirs "${_d}")
  endif()
endforeach()
set(_dirs "${_clean_dirs}")
list(LENGTH _dirs _n_dirs)
if(_n_dirs LESS 2)
  message(FATAL_ERROR "expected at least 2 C++ include dirs, got: ${_dirs}")
endif()
list(GET _dirs 0 _glibcxx_dir)
list(GET _dirs 1 _glibcxx_arch_dir)

set(FS_SHIM_EARLY "${CMAKE_CURRENT_SOURCE_DIR}/include/shim-early")
set(FS_SHIM_LATE "${CMAKE_CURRENT_SOURCE_DIR}/include/shim")

set(FS_COMPILE_FLAGS
  "--target=wasm32-unknown-unknown"
  "-nostdlib" "-ffreestanding" "-fno-exceptions" "-fno-rtti"
  "-fno-builtin" "-Oz" "-std=c++23"
  "-D__STDC_HOSTED__=1"
  "-nostdinc++"
  "-isystem${FS_SHIM_EARLY}"
  "-isystem${_glibcxx_dir}"
  "-isystem${_glibcxx_arch_dir}"
  "-isystem${FS_SHIM_LATE}"
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
file(GLOB_RECURSE FS_SHIM_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

add_custom_command(
  OUTPUT "${FS_GUEST_WASM}"
  COMMAND ${FS_CLANG} ${FS_COMPILE_FLAGS} ${FS_LINK_FLAGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/guest.cpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/libstdcxx_support.cpp"
          -o "${FS_GUEST_WASM}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/guest.cpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/libstdcxx_support.cpp"
          ${FS_SHIM_FILES}
  VERBATIM
)
add_custom_target(yase-json-guest ALL DEPENDS "${FS_GUEST_WASM}")

add_subdirectory(smoke)
