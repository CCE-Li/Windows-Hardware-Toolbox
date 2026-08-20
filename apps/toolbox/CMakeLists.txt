# Windows icon resource.
#
# The resource compiler shipped with this SDK is old and both (a) resolves
# referenced files (the .ico) relative to its working directory, and (b) breaks
# on the C++ compile definitions (e.g. -DHTB_VERSION_STRING=\"...\") that CMake
# would otherwise forward to rc.exe. We therefore generate a .rc whose icon path
# is absolute (forward-slash) and compile it with a dedicated custom command that
# carries no C++ defines.
set(ICON_PATH "${CMAKE_SOURCE_DIR}/resources/icon.ico")
file(TO_CMAKE_PATH "${ICON_PATH}" ICON_PATH)
configure_file(
    ${CMAKE_SOURCE_DIR}/resources/resources.rc.in
    ${CMAKE_BINARY_DIR}/resources/resources.rc
    @ONLY)

set(GENERATED_RC "${CMAKE_BINARY_DIR}/resources/resources.rc")
set(GENERATED_RES "${CMAKE_BINARY_DIR}/resources/resources.res")
add_custom_command(
    OUTPUT ${GENERATED_RES}
    COMMAND "${CMAKE_RC_COMPILER}" /nologo /fo "${GENERATED_RES}" "${GENERATED_RC}"
    DEPENDS ${GENERATED_RC}
    VERBATIM)

add_executable(toolbox WIN32 main.cpp ${GENERATED_RES})
target_link_libraries(toolbox PRIVATE htb_ui)
target_link_options(toolbox PRIVATE "${GENERATED_RES}")