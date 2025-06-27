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
    WhisperModel.cpp
)

# Add headers
set(SPIKE_HEADERS
    MinimalGGMLParser.h
    QuantizedBufferManager.h
    QuantizationDispatcher.h
    QuantizationSpike.h
    WhisperModel.h
)

# Add test executable for WhisperModel
set(TEST_WHISPER_SOURCES
    test_whisper_model.cpp
    WhisperModel.cpp
)

set(TEST_WHISPER_HEADERS
    WhisperModel.h
)

# Create main executable
add_executable(QuantizationSpike ${SPIKE_SOURCES} ${SPIKE_HEADERS})

# Create test executable for WhisperModel
add_executable(TestWhisperModel ${TEST_WHISPER_SOURCES} ${TEST_WHISPER_HEADERS})

# Link libraries
# For main spike (will add D3D11 libraries later)
# target_link_libraries(QuantizationSpike PRIVATE d3d11 dxgi)

# For WhisperModel test - link with whisper library
target_link_libraries(TestWhisperModel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Whisper/x64/Release/Whisper.lib
)

# Set output directory
set_target_properties(QuantizationSpike PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(TestWhisperModel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add compile definitions
target_compile_definitions(QuantizationSpike PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_compile_definitions(TestWhisperModel PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Enable debug information
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(QuantizationSpike PRIVATE /Zi)
endif()
