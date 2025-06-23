@echo off

echo ========================================
echo WhisperDesktopNG - Whisper.cpp Static Library Builder
echo ========================================
echo.

REM Check if we're in the correct directory
if not exist "external\whisper.cpp" (
    echo ERROR: whisper.cpp submodule not found!
    echo Please run this script from the project root directory.
    echo Make sure you have initialized the git submodule:
    echo   git submodule update --init --recursive
    pause
    exit /b 1
)

REM Set build configuration
set BUILD_TYPE=Release
set BUILD_DIR=build_whisper\build
set INSTALL_DIR=build_whisper\install

REM Create build directory
echo Creating build directory...
if exist "%BUILD_DIR%" (
    echo Cleaning existing build directory...
    rmdir /s /q "%BUILD_DIR%"
)
mkdir "%BUILD_DIR%"

REM Create install directory
if exist "%INSTALL_DIR%" (
    echo Cleaning existing install directory...
    rmdir /s /q "%INSTALL_DIR%"
)
mkdir "%INSTALL_DIR%"

echo.
echo ========================================
echo Configuring CMake...
echo ========================================

REM Configure with CMake
cd "%BUILD_DIR%"
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX=../../%INSTALL_DIR% ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DWHISPER_BUILD_TESTS=OFF ^
    -DWHISPER_BUILD_EXAMPLES=OFF ^
    -DWHISPER_BUILD_SERVER=OFF ^
    -DWHISPER_CURL=OFF ^
    -DWHISPER_SDL2=OFF ^
    -DWHISPER_FFMPEG=OFF ^
    -DWHISPER_COREML=OFF ^
    -DWHISPER_OPENVINO=OFF ^
    -DGGML_STATIC=ON ^
    -DGGML_AVX=ON ^
    -DGGML_AVX2=ON ^
    -DGGML_FMA=ON ^
    -DGGML_F16C=ON ^
    -DGGML_CUDA=OFF ^
    -DGGML_OPENCL=OFF ^
    -DGGML_METAL=OFF ^
    -DGGML_VULKAN=OFF ^
    ..\..\build_whisper

if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    cd ..\..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Building static libraries...
echo ========================================

REM Build the project
msbuild WhisperCppBuild.sln /p:Configuration=%BUILD_TYPE% /p:Platform=x64 /m

if errorlevel 1 (
    echo ERROR: Build failed!
    cd ..\..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Installing libraries and headers...
echo ========================================

REM Install (copy files to install directory)
cmake --build . --config %BUILD_TYPE% --target install

cd ..\..

echo.
echo ========================================
echo Copying libraries to project directories...
echo ========================================

REM Create lib directory if it doesn't exist
if not exist "lib" mkdir "lib"
if not exist "lib\x64" mkdir "lib\x64"
if not exist "lib\x64\%BUILD_TYPE%" mkdir "lib\x64\%BUILD_TYPE%"

REM Copy static libraries
if exist "%BUILD_DIR%\lib\whisper.lib" (
    copy "%BUILD_DIR%\lib\whisper.lib" "lib\x64\%BUILD_TYPE%\"
    echo Copied whisper.lib
) else (
    echo WARNING: whisper.lib not found!
)

if exist "%BUILD_DIR%\lib\ggml.lib" (
    copy "%BUILD_DIR%\lib\ggml.lib" "lib\x64\%BUILD_TYPE%\"
    echo Copied ggml.lib
) else (
    echo WARNING: ggml.lib not found!
)

REM Copy headers
if not exist "include" mkdir "include"
if not exist "include\whisper_cpp" mkdir "include\whisper_cpp"

if exist "%BUILD_DIR%\include" (
    xcopy "%BUILD_DIR%\include\*" "include\whisper_cpp\" /s /e /y
    echo Copied header files
) else (
    echo WARNING: Header files not found!
)

echo.
echo ========================================
echo Build Summary
echo ========================================
echo Build Type: %BUILD_TYPE%
echo Build Directory: %BUILD_DIR%
echo Install Directory: %INSTALL_DIR%
echo.

REM List generated files
echo Generated files:
if exist "lib\x64\%BUILD_TYPE%\whisper.lib" (
    echo   ✓ lib\x64\%BUILD_TYPE%\whisper.lib
) else (
    echo   ✗ lib\x64\%BUILD_TYPE%\whisper.lib [MISSING]
)

if exist "lib\x64\%BUILD_TYPE%\ggml.lib" (
    echo   ✓ lib\x64\%BUILD_TYPE%\ggml.lib
) else (
    echo   ✗ lib\x64\%BUILD_TYPE%\ggml.lib [MISSING]
)

if exist "include\whisper_cpp\whisper.h" (
    echo   ✓ include\whisper_cpp\whisper.h
) else (
    echo   ✗ include\whisper_cpp\whisper.h [MISSING]
)

if exist "include\whisper_cpp\ggml.h" (
    echo   ✓ include\whisper_cpp\ggml.h
) else (
    echo   ✗ include\whisper_cpp\ggml.h [MISSING]
)

echo.
echo ========================================
echo Build completed!
echo ========================================
echo.
echo You can now integrate these static libraries into your Visual Studio project.
echo.
pause
