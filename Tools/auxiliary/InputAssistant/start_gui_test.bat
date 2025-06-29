@echo off
cd /d "%~dp0"
echo Starting Input Assistant GUI Test...
echo Current directory: %CD%
echo.
echo Instructions:
echo 1. Click "Select Window" button
echo 2. Check if window list appears
echo 3. Look for "Found X windows" in top right
echo 4. Try to select a window
echo.
echo If it crashes or shows empty list, check Visual Studio Output window for debug messages.
echo.
pause
echo.
echo Starting GUI...
echo Looking for: %CD%\bin\Release\InputAssistant.exe
if exist "bin\Release\InputAssistant.exe" (
    echo File found, starting...
    start "" "bin\Release\InputAssistant.exe"
    echo GUI started. Test the window selection feature.
) else (
    echo ERROR: InputAssistant.exe not found!
    echo Please build the project first with: dotnet build --configuration Release
    dir bin\Release\*.exe 2>nul
)
pause
