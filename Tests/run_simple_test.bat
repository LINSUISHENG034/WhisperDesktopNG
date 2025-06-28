@echo off
echo Setting up Visual Studio environment...
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo Compiling simple test...
cd /d "%~dp0"
cl /EHsc /I.. simple_test.cpp /Fe:simple_test.exe

if %ERRORLEVEL% EQU 0 (
    echo Compilation successful! Running test...
    simple_test.exe
) else (
    echo Compilation failed!
    exit /b 1
)
