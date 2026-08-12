cmake_minimum_required(VERSION 3.25)

project(WindowsHardwareToolbox
    VERSION 0.1.0
    DESCRIPTION "Native Windows hardware management and diagnostics toolbox"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(HTB_BUILD_TESTS "Build unit tests" ON)

set(HTB_DEPS_DIR "${CMAKE_SOURCE_DIR}/.deps" CACHE PATH "Vendored third-party dependencies (see tools/fetch_deps.ps1)")
if(NOT EXISTS "${HTB_DEPS_DIR}/imgui/imgui.h" OR
   NOT EXISTS "${HTB_DEPS_DIR}/spdlog/src/spdlog.cpp" OR
   NOT EXISTS "${HTB_DEPS_DIR}/tomlplusplus/include/toml++/toml.hpp" OR
   NOT EXISTS "${HTB_DEPS_DIR}/googletest/CMakeLists.txt")
    message(FATAL_ERROR "Third-party dependencies not found in ${HTB_DEPS_DIR}. Run: powershell -ExecutionPolicy Bypass -File tools/fetch_deps.ps1")
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00 WINVER=0x0A00)
else()
    add_compile_options(-Wall -Wextra)
endif()

add_compile_definitions(HTB_VERSION_STRING=\"${PROJECT_VERSION}\")

include(cmake/third_party.cmake)

add_subdirectory(core)
add_subdirectory(hardware)
add_subdirectory(services)
add_subdirectory(ui)
add_subdirectory(apps)

if(HTB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
