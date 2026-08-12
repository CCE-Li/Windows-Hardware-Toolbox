add_library(htb_services STATIC HardwareService.cpp)
target_include_directories(htb_services PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
target_link_libraries(htb_services PUBLIC htb_hardware htb_core)
