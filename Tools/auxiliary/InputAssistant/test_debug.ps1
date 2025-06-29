# Input Assistant Debug Test Script

Write-Host "===== Input Assistant Debug Test =====" -ForegroundColor Green
Write-Host ""

Write-Host "This script will help debug the window selection issue." -ForegroundColor Yellow
Write-Host ""

Write-Host "Test Steps:" -ForegroundColor Cyan
Write-Host "1. First run the command-line debugger to verify window enumeration works"
Write-Host "2. Then run the GUI application to test window selection"
Write-Host "3. Compare results to identify the issue"
Write-Host ""

$choice = Read-Host "Press 1 for Command-line Debugger, 2 for GUI App, or Enter to exit"

switch ($choice) {
    "1" {
        Write-Host "Starting Command-line Window Debugger..." -ForegroundColor Green
        Write-Host "This will show if window enumeration works correctly." -ForegroundColor Yellow
        Write-Host ""
        
        $debuggerPath = "Tools\auxiliary\WindowDebugger\bin\Release\WindowDebugger.exe"
        if (Test-Path $debuggerPath) {
            Start-Process -FilePath $debuggerPath -Wait
        } else {
            Write-Host "Debugger not found at: $debuggerPath" -ForegroundColor Red
            Write-Host "Please build the WindowDebugger project first." -ForegroundColor Red
        }
    }
    
    "2" {
        Write-Host "Starting GUI Input Assistant..." -ForegroundColor Green
        Write-Host "Test the window selection feature." -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Instructions:" -ForegroundColor Cyan
        Write-Host "- Click 'Select Window' button"
        Write-Host "- Check if window list appears"
        Write-Host "- Look for error messages"
        Write-Host "- Note if the app crashes or exits"
        Write-Host ""
        
        $guiPath = "Tools\auxiliary\InputAssistant\bin\Release\InputAssistant.exe"
        if (Test-Path $guiPath) {
            Start-Process -FilePath $guiPath
            Write-Host "GUI application started. Check the window selection feature." -ForegroundColor Green
        } else {
            Write-Host "GUI app not found at: $guiPath" -ForegroundColor Red
            Write-Host "Please build the InputAssistant project first." -ForegroundColor Red
        }
    }
    
    default {
        Write-Host "Exiting..." -ForegroundColor Gray
    }
}

Write-Host ""
Write-Host "Debug Information:" -ForegroundColor Cyan
Write-Host "- If command-line debugger works but GUI doesn't, the issue is in GUI implementation"
Write-Host "- If both fail, the issue is in the core window enumeration logic"
Write-Host "- Check Visual Studio Output window for debug messages when running GUI"
Write-Host ""

Read-Host "Press Enter to exit"
