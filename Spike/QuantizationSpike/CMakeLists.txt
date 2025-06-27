cmake_minimum_required(VERSION 3.16)
project(QuantizationSpike)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../Whisper)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../ComLightLib)

# Add source files
set(SPIKE_SOURCES
    main.cpp
    MinimalGGMLParser.cpp
    QuantizedBufferManager.cpp
    QuantizationDispatcher.cpp
    QuantizationSpike.cpp
)

# Add headers
set(SPIKE_HEADERS
    MinimalGGMLParser.h
    QuantizedBufferManager.h
    QuantizationDispatcher.h
    QuantizationSpike.h
)

# Create executable
add_executable(QuantizationSpike ${SPIKE_SOURCES} ${SPIKE_HEADERS})

# Link libraries (we'll add these as we progress)
# target_link_libraries(QuantizationSpike PRIVATE d3d11 dxgi)

# Set output directory
set_target_properties(QuantizationSpike PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add compile definitions
target_compile_definitions(QuantizationSpike PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Enable debug information
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(QuantizationSpike PRIVATE /Zi)
endif()
