add_executable(toolbox WIN32 main.cpp)
target_link_libraries(toolbox PRIVATE htb_ui)
add_custom_command(TARGET toolbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/apps/camera_engine/camera_engine.py $<TARGET_FILE_DIR:toolbox>/camera_engine.py)
