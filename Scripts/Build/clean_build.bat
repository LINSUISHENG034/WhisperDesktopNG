@echo off
REM WhisperDesktopNG - 构建清理脚本
REM 清理所有构建产生的临时文件和目录

echo ========================================
echo WhisperDesktopNG - 构建清理脚本
echo ========================================
echo.

echo 正在清理构建文件...

REM 切换到项目根目录
cd ..\..\

REM 清理构建目录
if exist "Scripts\Build\whisper_build\build" (
    echo 删除 Scripts\Build\whisper_build\build 目录...
    rmdir /s /q "Scripts\Build\whisper_build\build"
)

REM 清理生成的库文件
if exist "lib\x64\Release\whisper.lib" (
    echo 删除生成的库文件...
    del /q "lib\x64\Release\whisper*.lib"
    del /q "lib\x64\Release\ggml*.lib"
)

REM 清理头文件
if exist "include\whisper_cpp" (
    echo 删除复制的头文件...
    rmdir /s /q "include\whisper_cpp"
)

REM 清理 Visual Studio 临时文件
if exist "x64" (
    echo 清理 Visual Studio 输出目录...
    rmdir /s /q "x64"
)

REM 清理各项目的临时文件
for /d %%i in (Whisper ComLightLib ComputeShaders Examples\* Tools\* WhisperNet WhisperPS) do (
    if exist "%%i\x64" (
        echo 清理 %%i\x64...
        rmdir /s /q "%%i\x64"
    )
    if exist "%%i\obj" (
        echo 清理 %%i\obj...
        rmdir /s /q "%%i\obj"
    )
    if exist "%%i\bin" (
        echo 清理 %%i\bin...
        rmdir /s /q "%%i\bin"
    )
)

echo.
echo ========================================
echo 清理完成！
echo ========================================
echo.
echo 现在可以重新运行构建脚本进行干净的构建。
echo.
pause
