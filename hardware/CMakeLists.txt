add_library(htb_hardware STATIC
    cpu/CpuProvider.cpp
    gpu/GpuProvider.cpp
    memory/MemoryProvider.cpp
    wmi/WmiSession.cpp
)
target_include_directories(htb_hardware PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
target_link_libraries(htb_hardware
    PUBLIC htb_core
    PRIVATE pdh dxgi wbemuuid ole32 oleaut32)
