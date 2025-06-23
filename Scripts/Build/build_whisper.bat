@echo off

echo ========================================
echo Building Whisper.cpp Static Libraries
echo ========================================

REM Check if running from project root or Scripts\Build
if exist "external\whisper.cpp" (
    set PROJECT_ROOT=.
) else if exist "..\..\external\whisper.cpp" (
    set PROJECT_ROOT=..\..
) else (
    echo ERROR: whisper.cpp submodule not found!
    echo Please run: git submodule update --init --recursive
    echo Make sure to run this script from the project root or Scripts\Build directory
    pause
    exit /b 1
)

set BUILD_TYPE=Release
if "%PROJECT_ROOT%"=="." (
    set BUILD_DIR=Scripts\Build\whisper_build\build
) else (
    set BUILD_DIR=whisper_build\build
)

echo Creating build directory...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

echo Configuring CMake...

if "%PROJECT_ROOT%"=="." (
    "E:\Program Files\CMake\bin\cmake.exe" -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DBUILD_SHARED_LIBS=OFF -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_EXAMPLES=OFF -DWHISPER_BUILD_SERVER=OFF -S Scripts\Build\whisper_build -B Scripts\Build\whisper_build\build
) else (
    "E:\Program Files\CMake\bin\cmake.exe" -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DBUILD_SHARED_LIBS=OFF -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_EXAMPLES=OFF -DWHISPER_BUILD_SERVER=OFF -S whisper_build -B whisper_build\build
)

if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    cd %PROJECT_ROOT%
    pause
    exit /b 1
)

echo Building...
if "%PROJECT_ROOT%"=="." (
    "E:\Program Files\CMake\bin\cmake.exe" --build Scripts\Build\whisper_build\build --config %BUILD_TYPE% --parallel
) else (
    "E:\Program Files\CMake\bin\cmake.exe" --build whisper_build\build --config %BUILD_TYPE% --parallel
)

if errorlevel 1 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo Creating lib directories...
if not exist "lib" mkdir "lib"
if not exist "lib\x64" mkdir "lib\x64"
if not exist "lib\x64\%BUILD_TYPE%" mkdir "lib\x64\%BUILD_TYPE%"

echo Copying libraries...
if exist "%BUILD_DIR%\lib\%BUILD_TYPE%\whisper.lib" (
    copy "%BUILD_DIR%\lib\%BUILD_TYPE%\whisper.lib" "lib\x64\%BUILD_TYPE%\"
    echo Copied whisper.lib
)

if exist "%BUILD_DIR%\lib\%BUILD_TYPE%\ggml.lib" (
    copy "%BUILD_DIR%\lib\%BUILD_TYPE%\ggml.lib" "lib\x64\%BUILD_TYPE%\"
    echo Copied ggml.lib
)

if exist "%BUILD_DIR%\lib\%BUILD_TYPE%\ggml-base.lib" (
    copy "%BUILD_DIR%\lib\%BUILD_TYPE%\ggml-base.lib" "lib\x64\%BUILD_TYPE%\"
    echo Copied ggml-base.lib
)

if exist "%BUILD_DIR%\lib\%BUILD_TYPE%\ggml-cpu.lib" (
    copy "%BUILD_DIR%\lib\%BUILD_TYPE%\ggml-cpu.lib" "lib\x64\%BUILD_TYPE%\"
    echo Copied ggml-cpu.lib
)

echo Copying headers...
if not exist "include" mkdir "include"
if not exist "include\whisper_cpp" mkdir "include\whisper_cpp"

if exist "%BUILD_DIR%\include" (
    xcopy "%BUILD_DIR%\include\*" "include\whisper_cpp\" /s /e /y
    echo Copied headers
)

echo Build completed!
pause
