# Simple WhisperDesktopNG Test Script

Write-Host "=== WhisperDesktopNG Simple Test ===" -ForegroundColor Green

# Test 1: Check executable
$exePath = "x64\Release\WhisperDesktop.exe"
if (Test-Path $exePath) {
    $fileInfo = Get-Item $exePath
    Write-Host "✓ Executable exists: $($fileInfo.Length) bytes" -ForegroundColor Green
} else {
    Write-Host "✗ Executable not found" -ForegroundColor Red
    exit 1
}

# Test 2: Check DLL
$dllPath = "x64\Release\Whisper.dll"
if (Test-Path $dllPath) {
    Write-Host "✓ Whisper.dll exists" -ForegroundColor Green
} else {
    Write-Host "✗ Whisper.dll not found" -ForegroundColor Red
    exit 1
}

Write-Host "✓ All tests passed!" -ForegroundColor Green
