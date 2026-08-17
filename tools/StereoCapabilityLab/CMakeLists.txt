cmake_minimum_required(VERSION 3.24)

project(StereoCapabilityLab VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "StereoCapabilityLab requires Windows and Direct3D 11.")
endif()

execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE STEREO_LAB_GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT STEREO_LAB_GIT_SHA)
    set(STEREO_LAB_GIT_SHA "unknown")
endif()

string(TIMESTAMP STEREO_LAB_BUILD_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)

add_executable(StereoCapabilityLab
    src/Main.cpp
    src/Support.cpp
    src/D3D11Lab.cpp
    src/Lab.h)

target_compile_features(StereoCapabilityLab PRIVATE cxx_std_20)
target_compile_definitions(StereoCapabilityLab PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    STEREO_LAB_VERSION="${PROJECT_VERSION}"
    STEREO_LAB_GIT_SHA="${STEREO_LAB_GIT_SHA}"
    STEREO_LAB_BUILD_UTC="${STEREO_LAB_BUILD_UTC}")

if(MSVC)
    target_compile_options(StereoCapabilityLab PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

target_link_libraries(StereoCapabilityLab PRIVATE
    d3d11
    dxgi
    d3dcompiler
    bcrypt
    powrprof
    ole32)

add_custom_command(TARGET StereoCapabilityLab POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:StereoCapabilityLab>/shaders"
    COMMENT "Copying runtime HLSL shaders")

install(TARGETS StereoCapabilityLab RUNTIME DESTINATION .)
install(DIRECTORY shaders DESTINATION .)
