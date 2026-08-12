add_library(htb_imgui STATIC
    ${HTB_DEPS_DIR}/imgui/imgui.cpp
    ${HTB_DEPS_DIR}/imgui/imgui_demo.cpp
    ${HTB_DEPS_DIR}/imgui/imgui_draw.cpp
    ${HTB_DEPS_DIR}/imgui/imgui_tables.cpp
    ${HTB_DEPS_DIR}/imgui/imgui_widgets.cpp
    ${HTB_DEPS_DIR}/imgui/backends/imgui_impl_win32.cpp
    ${HTB_DEPS_DIR}/imgui/backends/imgui_impl_dx11.cpp
)
target_include_directories(htb_imgui
    PUBLIC ${HTB_DEPS_DIR}/imgui ${HTB_DEPS_DIR}/imgui/backends)
target_compile_options(htb_imgui PRIVATE /W3)
target_link_libraries(htb_imgui PUBLIC d3d11 dxgi d3dcompiler)

add_library(htb_spdlog STATIC
    ${HTB_DEPS_DIR}/spdlog/src/async.cpp
    ${HTB_DEPS_DIR}/spdlog/src/bundled_fmtlib_format.cpp
    ${HTB_DEPS_DIR}/spdlog/src/cfg.cpp
    ${HTB_DEPS_DIR}/spdlog/src/color_sinks.cpp
    ${HTB_DEPS_DIR}/spdlog/src/file_sinks.cpp
    ${HTB_DEPS_DIR}/spdlog/src/spdlog.cpp
    ${HTB_DEPS_DIR}/spdlog/src/stdout_sinks.cpp
)
target_include_directories(htb_spdlog PUBLIC ${HTB_DEPS_DIR}/spdlog/include)
target_compile_definitions(htb_spdlog PUBLIC SPDLOG_COMPILED_LIB)
target_compile_options(htb_spdlog PRIVATE /W3)

add_library(htb_tomlplusplus INTERFACE)
target_include_directories(htb_tomlplusplus INTERFACE ${HTB_DEPS_DIR}/tomlplusplus/include)

if(HTB_BUILD_TESTS)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(gtest_build_samples OFF CACHE BOOL "" FORCE)
    set(gtest_build_tests OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(${HTB_DEPS_DIR}/googletest ${CMAKE_BINARY_DIR}/third_party/googletest EXCLUDE_FROM_ALL)
endif()
