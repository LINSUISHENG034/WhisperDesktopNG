@echo off
echo === WhisperCpp PCM Transcription Test ===

set PROJECT_ROOT=F:\Projects\WhisperDesktopNG
set MODEL_PATH=E:\Program Files\WhisperDesktop\ggml-tiny.bin

cd /d %PROJECT_ROOT%

echo 1. Building Whisper.dll...
msbuild "Whisper\Whisper.vcxproj" /p:Configuration=Debug /p:Platform=x64 /nologo
if %ERRORLEVEL% neq 0 (
    echo ERROR: Failed to build Whisper.dll
    exit /b 1
)

echo 2. Building main.exe...
msbuild "Examples\main\main.vcxproj" /p:Configuration=Debug /p:Platform=x64 /nologo
if %ERRORLEVEL% neq 0 (
    echo ERROR: Failed to build main.exe
    exit /b 1
)

echo 3. Copying DLL...
copy "Whisper\x64\Debug\Whisper.dll" "Examples\main\x64\Debug\" /Y

echo 4. Testing JFK audio...
cd "Examples\main\x64\Debug"
main.exe --test-pcm "%MODEL_PATH%" "F:\Projects\WhisperDesktopNG\SampleClips\jfk.wav"
set JFK_RESULT=%ERRORLEVEL%

echo.
echo 5. Testing Columbia audio...
main.exe --test-pcm "%MODEL_PATH%" "F:\Projects\WhisperDesktopNG\SampleClips\columbia_converted.wav"
set COLUMBIA_RESULT=%ERRORLEVEL%

echo.
echo === Test Results ===
if %JFK_RESULT% equ 0 (
    echo JFK audio: SUCCESS
) else (
    echo JFK audio: FAILED
)

if %COLUMBIA_RESULT% equ 0 (
    echo Columbia audio: SUCCESS
) else (
    echo Columbia audio: FAILED
)

if %JFK_RESULT% equ 0 if %COLUMBIA_RESULT% equ 0 (
    echo.
    echo All tests PASSED!
    exit /b 0
) else (
    echo.
    echo Some tests FAILED
    exit /b 1
)
