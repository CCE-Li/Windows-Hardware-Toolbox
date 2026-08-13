add_library(virtualcamera SHARED VirtualCameraProvider.cpp virtualcamera.def)
target_link_libraries(virtualcamera PRIVATE mfplat mfuuid ole32)
