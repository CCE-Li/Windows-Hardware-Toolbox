add_library(htb_ui STATIC
    UiApp.cpp
    themes/Theme.cpp
    pages/DashboardPage.cpp
    pages/CpuPage.cpp
    pages/GpuPage.cpp
    pages/MemoryPage.cpp
)
target_include_directories(htb_ui PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
target_link_libraries(htb_ui PUBLIC htb_imgui htb_services htb_core)
