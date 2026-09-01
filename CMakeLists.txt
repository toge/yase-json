cmake_minimum_required(VERSION 3.25)
project(yase-json LANGUAGES CXX)

option(BUILD_TEST "Enable tests" ON)
option(BUILD_EXAMPLE "Enable examples" ON)
option(BUILD_BENCH "Enable benchmarks" OFF)
option(YASE_JSON_FREESTANDING "Build wasm32-unknown-unknown freestanding guest" OFF)

## add warning
if (CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g3")
endif()

if (MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
endif()

## optimization
if (CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")
endif()
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Ox")
endif()

## other flag
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(glaze REQUIRED CONFIG)

# --- Library Target ---
add_library(yase-json INTERFACE)
add_library(yase-json::yase-json ALIAS yase-json)

target_include_directories(yase-json INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(yase-json INTERFACE cxx_std_23)
target_link_libraries(yase-json INTERFACE glaze::glaze)

# --- Installer ---
include(GNUInstallDirs)

install(TARGETS yase-json
    EXPORT yase-json-targets
)

install(DIRECTORY include/yase-json
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT yase-json-targets
    FILE yase-json-targets.cmake
    NAMESPACE yase-json::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/yase-json
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "yase-json-config-version.cmake"
    VERSION 0.1.0
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/yase-json-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/yase-json-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/yase-json
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/yase-json-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/yase-json-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/yase-json
)

# --- Tests & Examples ---
if(BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif()

if(BUILD_BENCH)
    add_executable(benchmark test/benchmark.cpp)
    target_link_libraries(benchmark PRIVATE yase-json::yase-json)
    target_compile_features(benchmark PRIVATE cxx_std_23)
endif()

if(BUILD_EXAMPLE)
    add_executable(example example.cpp)
    target_link_libraries(example PRIVATE yase-json::yase-json)
    target_compile_features(example PRIVATE cxx_std_23)
endif()

if(YASE_JSON_FREESTANDING)
    add_subdirectory(freestanding)
endif()
