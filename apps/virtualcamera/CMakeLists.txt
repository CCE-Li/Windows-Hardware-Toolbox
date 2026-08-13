add_library(virtualcamera SHARED VirtualCameraProvider.cpp virtualcamera.def)
target_include_directories(virtualcamera PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(virtualcamera PRIVATE mfplat mfuuid ole32)
