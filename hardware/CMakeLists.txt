add_library(htb_hardware STATIC
    audio/AudioProvider.cpp
    battery/BatteryProvider.cpp
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
    storage/StorageProvider.cpp
    wmi/WmiSession.cpp
)
target_include_directories(htb_hardware PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
target_link_libraries(htb_hardware
    PUBLIC htb_core
    PRIVATE pdh dxgi wbemuuid ole32 oleaut32 setupapi iphlpapi ws2_32 mfplat Mfcore mfuuid mfsensorgroup mfreadwrite)
