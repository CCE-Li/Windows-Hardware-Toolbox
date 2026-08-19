add_library(htb_hardware STATIC
    audio/AudioProvider.cpp
    battery/BatteryProvider.cpp
    camera/CameraEngineController.cpp
    camera/CameraProvider.cpp
    camera/ObsVirtualCam.cpp
    camera/VideoTransformPipeline.cpp
    camera/VirtualCameraController.cpp
    camera/VirtualCameraRegistrar.cpp
    cpu/CpuProvider.cpp
    device/DeviceProvider.cpp
    display/DisplayProvider.cpp
    gpu/GpuProvider.cpp
    memory/MemoryProvider.cpp
    network/NetworkProvider.cpp
    process/ProcessProvider.cpp
    sensors/SensorsProvider.cpp
    startup/StartupProvider.cpp
    storage/StorageProvider.cpp
    systemservice/SystemServiceProvider.cpp
    wmi/WmiSession.cpp
)
target_include_directories(htb_hardware PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
target_compile_options(htb_hardware PRIVATE /EHa)
target_link_libraries(htb_hardware
    PUBLIC htb_core
    PRIVATE pdh dxgi wbemuuid ole32 oleaut32 setupapi iphlpapi ws2_32 mfplat Mfcore mfuuid mfsensorgroup mfreadwrite
            ntdll psapi advapi32 shell32 gdi32 user32)
