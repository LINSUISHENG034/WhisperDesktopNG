cmake_minimum_required(VERSION 3.15)
project(WhisperCppBuild VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Force static library build
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

# Configure whisper.cpp options for static library build
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "Build server" FORCE)

# Disable unnecessary features for Windows static build
set(WHISPER_CURL OFF CACHE BOOL "Use libcurl" FORCE)
set(WHISPER_SDL2 OFF CACHE BOOL "Use SDL2" FORCE)
set(WHISPER_FFMPEG OFF CACHE BOOL "Use FFmpeg" FORCE)
set(WHISPER_COREML OFF CACHE BOOL "Use CoreML" FORCE)
set(WHISPER_OPENVINO OFF CACHE BOOL "Use OpenVINO" FORCE)

# GGML options for static build
set(GGML_STATIC ON CACHE BOOL "Build static GGML library" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "Enable native optimizations" FORCE)
set(GGML_AVX ON CACHE BOOL "Enable AVX" FORCE)
set(GGML_AVX2 ON CACHE BOOL "Enable AVX2" FORCE)
set(GGML_FMA ON CACHE BOOL "Enable FMA" FORCE)
set(GGML_F16C ON CACHE BOOL "Enable F16C" FORCE)

# Disable GPU backends for simplicity (can be enabled later if needed)
set(GGML_CUDA OFF CACHE BOOL "Enable CUDA" FORCE)
set(GGML_OPENCL OFF CACHE BOOL "Enable OpenCL" FORCE)
set(GGML_METAL OFF CACHE BOOL "Enable Metal" FORCE)
set(GGML_VULKAN OFF CACHE BOOL "Enable Vulkan" FORCE)

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add the whisper.cpp subdirectory
add_subdirectory(../../../external/whisper.cpp ${CMAKE_BINARY_DIR}/whisper.cpp)

# Create a custom target to copy headers
add_custom_target(copy_headers ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/../../../external/whisper.cpp/include
        ${CMAKE_BINARY_DIR}/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/../../../external/whisper.cpp/ggml/include
        ${CMAKE_BINARY_DIR}/include
    COMMENT "Copying header files"
)

# Make sure headers are copied before building
add_dependencies(whisper copy_headers)
add_dependencies(ggml copy_headers)

# Print build information
message(STATUS "WhisperCpp Build Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Static Libraries: ${BUILD_SHARED_LIBS}")
message(STATUS "  Output Directory: ${CMAKE_BINARY_DIR}")
message(STATUS "  Archive Output: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
