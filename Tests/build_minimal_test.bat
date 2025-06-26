@echo off
echo Building minimal whisper.cpp test...

REM 设置编译器路径
set MSVC_PATH="D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64"
set INCLUDE_PATHS=/I"../include" /I"../external/whisper.cpp" /I"../external/whisper.cpp/examples"

REM 编译
%MSVC_PATH%\cl.exe /EHsc /std:c++17 %INCLUDE_PATHS% ^
    minimal_whisper_test.cpp ^
    ../external/whisper.cpp/build/src/Release/whisper.lib ^
    ../external/whisper.cpp/examples/common.cpp ^
    /Fe:minimal_test.exe

if %ERRORLEVEL% EQU 0 (
    echo Build successful!
    echo Running test...
    minimal_test.exe
) else (
    echo Build failed!
)

pause
