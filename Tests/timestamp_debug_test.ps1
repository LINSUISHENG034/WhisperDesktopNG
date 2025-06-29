# Timestamp Generation Debug Test
# Purpose: Collect detailed information about timestamp generation failure

param(
    [string]$AudioFile = "Tests\Audio\zh_medium_audio.mp3",
    [string]$ModelFile = "Tests\Models\ggml-small.bin",
    [string]$OutputDir = "Tests\Results"
)

Write-Host "=== Timestamp Generation Debug Test ===" -ForegroundColor Cyan
Write-Host "Audio File: $AudioFile" -ForegroundColor Yellow
Write-Host "Model File: $ModelFile" -ForegroundColor Yellow
Write-Host "Output Directory: $OutputDir" -ForegroundColor Yellow

# Create output directory
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Test 1: Timestamp mode (should fail)
Write-Host "`n=== Test 1: Timestamp Mode ===" -ForegroundColor Magenta
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outputFile1 = "$OutputDir\debug_timestamp_$timestamp.txt"

$args1 = @(
    "-m", "`"$ModelFile`""
    "-f", "`"$AudioFile`""
    "-l", "auto"
    "-otxt"
    "-op", "`"$outputFile1`""
)

Write-Host "Command: main.exe $($args1 -join ' ')" -ForegroundColor Gray
$result1 = & "Examples\main\x64\Release\main.exe" @args1 2>&1
$exitCode1 = $LASTEXITCODE

Write-Host "Exit Code: $exitCode1" -ForegroundColor $(if($exitCode1 -eq 0){"Green"}else{"Red"})
if (Test-Path $outputFile1) {
    $content1 = Get-Content $outputFile1 -Raw
    Write-Host "Output Length: $($content1.Length) characters" -ForegroundColor Gray
    if ($content1 -match "Timestamp generation failed") {
        Write-Host "Result: TIMESTAMP GENERATION FAILED" -ForegroundColor Red
    } else {
        Write-Host "Result: SUCCESS" -ForegroundColor Green
    }
} else {
    Write-Host "Result: NO OUTPUT FILE GENERATED" -ForegroundColor Red
}

# Test 2: No-timestamp mode (should work)
Write-Host "`n=== Test 2: No-Timestamp Mode ===" -ForegroundColor Magenta
$outputFile2 = "$OutputDir\debug_notimestamp_$timestamp.txt"

$args2 = @(
    "-m", "`"$ModelFile`""
    "-f", "`"$AudioFile`""
    "-l", "auto"
    "-nt"
    "-otxt"
    "-op", "`"$outputFile2`""
)

Write-Host "Command: main.exe $($args2 -join ' ')" -ForegroundColor Gray
$result2 = & "Examples\main\x64\Release\main.exe" @args2 2>&1
$exitCode2 = $LASTEXITCODE

Write-Host "Exit Code: $exitCode2" -ForegroundColor $(if($exitCode2 -eq 0){"Green"}else{"Red"})
if (Test-Path $outputFile2) {
    $content2 = Get-Content $outputFile2 -Raw
    Write-Host "Output Length: $($content2.Length) characters" -ForegroundColor Gray
    if ($content2.Length -gt 10) {
        Write-Host "Result: SUCCESS - Generated meaningful content" -ForegroundColor Green
    } else {
        Write-Host "Result: MINIMAL OUTPUT" -ForegroundColor Yellow
    }
} else {
    Write-Host "Result: NO OUTPUT FILE GENERATED" -ForegroundColor Red
}

# Analysis
Write-Host "`n=== Analysis ===" -ForegroundColor Magenta

if (Test-Path $outputFile1) {
    Write-Host "`nTimestamp Mode Output:" -ForegroundColor Cyan
    Get-Content $outputFile1 | Select-Object -First 5 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
}

if (Test-Path $outputFile2) {
    Write-Host "`nNo-Timestamp Mode Output:" -ForegroundColor Cyan
    Get-Content $outputFile2 | Select-Object -First 5 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
}

# Debug log analysis
Write-Host "`n=== Debug Log Analysis ===" -ForegroundColor Magenta
$debugLines = $result1 | Where-Object { $_ -match "DEBUG:" -or $_ -match "runFullImpl:" -or $_ -match "failed to generate" }
if ($debugLines) {
    Write-Host "Key Debug Information:" -ForegroundColor Cyan
    $debugLines | Select-Object -Last 10 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
} else {
    Write-Host "No debug information found in output" -ForegroundColor Yellow
}

Write-Host "`n=== Summary ===" -ForegroundColor Magenta
Write-Host "Timestamp Mode: $(if($exitCode1 -eq 0){"✅ Process Success"}else{"❌ Process Failed"})" -ForegroundColor $(if($exitCode1 -eq 0){"Green"}else{"Red"})
Write-Host "No-Timestamp Mode: $(if($exitCode2 -eq 0){"✅ Process Success"}else{"❌ Process Failed"})" -ForegroundColor $(if($exitCode2 -eq 0){"Green"}else{"Red"})

if (Test-Path $outputFile1) {
    $hasTimestamps = (Get-Content $outputFile1 -Raw) -notmatch "Timestamp generation failed"
    Write-Host "Timestamp Generation: $(if($hasTimestamps){"✅ Working"}else{"❌ Failed"})" -ForegroundColor $(if($hasTimestamps){"Green"}else{"Red"})
}

Write-Host "`nGenerated Files:" -ForegroundColor Cyan
if (Test-Path $outputFile1) { Write-Host "  $outputFile1" -ForegroundColor Gray }
if (Test-Path $outputFile2) { Write-Host "  $outputFile2" -ForegroundColor Gray }

Write-Host "`nDebug test completed!" -ForegroundColor Green
