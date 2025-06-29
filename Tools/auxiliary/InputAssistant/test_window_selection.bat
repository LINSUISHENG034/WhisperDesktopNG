@echo off
echo ===== Input Assistant Window Selection Test =====
echo.
echo Test Steps:
echo 1. Launch Input Assistant
echo 2. Click "Select Window" button
echo 3. Check if window list displays
echo 4. Select a window for testing
echo.
echo Expected Results:
echo - Window selector displays normally
echo - Top right shows "Found X windows"
echo - List contains available application windows
echo - Can successfully select window
echo.
echo If problems occur:
echo - Click "Test Selector" button for diagnosis
echo - Click "Refresh List" to rescan windows
echo - Check debug tool output
echo.
pause
echo.
echo Starting Input Assistant...
start "" "bin\Release\InputAssistant.exe"
echo.
echo Debug tool available (optional)...
echo To debug, run in another command window:
echo Tools\auxiliary\WindowDebugger\bin\Release\WindowDebugger.exe
echo.
pause
