# Install script for directory: F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files/WhisperCppBuild")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("F:/Projects/WhisperDesktopNG/Scripts/Build/whisper_build/build/whisper.cpp/ggml/src/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "F:/Projects/WhisperDesktopNG/Scripts/Build/whisper_build/build/lib/Debug/ggml.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "F:/Projects/WhisperDesktopNG/Scripts/Build/whisper_build/build/lib/Release/ggml.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "F:/Projects/WhisperDesktopNG/Scripts/Build/whisper_build/build/lib/MinSizeRel/ggml.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "F:/Projects/WhisperDesktopNG/Scripts/Build/whisper_build/build/lib/RelWithDebInfo/ggml.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-cpu.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-alloc.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-backend.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-blas.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-cann.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-cpp.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-cuda.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-kompute.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-opt.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-metal.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-rpc.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-sycl.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/ggml-vulkan.h"
    "F:/Projects/WhisperDesktopNG/external/whisper.cpp/ggml/include/gguf.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "F:/Projects/WhisperDesktopNG/Scripts/Build/whisper_build/build/lib/Debug/ggml-base.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "F:/Projects/WhisperDesktopNG/Scripts/Build/whisper_build/build/lib/Release/ggml-base.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "F:/Projects/WhisperDesktopNG/Scripts/Build/whisper_build/build/lib/MinSizeRel/ggml-base.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "F:/Projects/WhisperDesktopNG/Scripts/Build/whisper_build/build/lib/RelWithDebInfo/ggml-base.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ggml" TYPE FILE FILES
    "F:/Projects/WhisperDesktopNG/Scripts/Build/whisper_build/build/whisper.cpp/ggml/ggml-config.cmake"
    "F:/Projects/WhisperDesktopNG/Scripts/Build/whisper_build/build/whisper.cpp/ggml/ggml-version.cmake"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "F:/Projects/WhisperDesktopNG/Scripts/Build/whisper_build/build/whisper.cpp/ggml/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
