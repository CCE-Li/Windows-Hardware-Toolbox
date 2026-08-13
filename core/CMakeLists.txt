add_library(htb_core STATIC
    logging/Logger.cpp
    config/Config.cpp
    runtime/SystemInfo.cpp
    util/Clipboard.cpp
    util/Utf.cpp
)
target_include_directories(htb_core PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
target_link_libraries(htb_core PUBLIC htb_spdlog htb_tomlplusplus)
