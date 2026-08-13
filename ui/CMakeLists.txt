add_library(htb_ui STATIC
    UiApp.cpp
    themes/Theme.cpp
    pages/AboutPage.cpp
    pages/AudioPage.cpp
    pages/CameraPage.cpp
    pages/CpuPage.cpp
    pages/DashboardPage.cpp
    pages/DevicesPage.cpp
    pages/DiagnosticsPage.cpp
    pages/DisplayPage.cpp
    pages/GpuPage.cpp
    pages/MemoryPage.cpp
    pages/NetworkPage.cpp
    pages/SensorsPage.cpp
    pages/StoragePage.cpp
    pages/UsbPage.cpp
)
target_include_directories(htb_ui PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
target_link_libraries(htb_ui PUBLIC htb_imgui htb_services htb_core)
