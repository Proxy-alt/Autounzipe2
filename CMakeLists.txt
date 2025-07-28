cmake_minimum_required(VERSION 3.16)
project(AutoUnzipService)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add executable first
add_executable(AutoUnzipService WIN32
    AutoUnzipService.cpp
    resource.rc
)

# Set Windows subsystem to avoid console window
if(WIN32)
    set_property(TARGET AutoUnzipService PROPERTY WIN32_EXECUTABLE TRUE)
    set_property(TARGET AutoUnzipService PROPERTY LINK_FLAGS "/SUBSYSTEM:WINDOWS")
endif()

# Link required libraries
target_link_libraries(AutoUnzipService
    shell32
    comctl32
    advapi32
    user32
    kernel32
    gdi32
    ole32
    oleaut32
)

# Add resource compilation
if(WIN32)
    enable_language(RC)
    set_property(SOURCE resource.rc PROPERTY LANGUAGE RC)
endif()

# Compiler-specific options
if(MSVC)
    target_compile_definitions(AutoUnzipService PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        _MBCS
    )
    
    # Use ANSI (not Unicode) for compatibility
    # target_compile_definitions(AutoUnzipService PRIVATE
    #     UNICODE
    #     _UNICODE
    # )
    
    # Set runtime library
    set_property(TARGET AutoUnzipService PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Install target
install(TARGETS AutoUnzipService
    RUNTIME DESTINATION bin
)

# CPack configuration for MSI generation
set(CPACK_GENERATOR "WIX")
set(CPACK_PACKAGE_NAME "Auto Unzip Service")
set(CPACK_PACKAGE_VENDOR "Auto Unzip Service")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Automatic archive extraction service for Windows")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")

# WiX-specific settings
set(CPACK_WIX_UPGRADE_GUID "12345678-1234-1234-1234-123456789012")
set(CPACK_WIX_PRODUCT_GUID "87654321-4321-4321-4321-210987654321")
set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/tray_icon.ico")

# Add custom WiX fragment for service installation
set(CPACK_WIX_EXTRA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/service_install.wxs")

include(CPack)