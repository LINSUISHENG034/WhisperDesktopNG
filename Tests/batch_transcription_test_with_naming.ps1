# Enhanced Batch Transcription Test with Custom Naming
# This script tests multiple models against multiple audio files with proper result organization

# Enhanced Batch Transcription Test Script
# Usage Examples:
#   .\batch_transcription_test_with_naming.ps1                    # Auto language detection
#   .\batch_transcription_test_with_naming.ps1 -Language "en"     # English only
#   .\batch_transcription_test_with_naming.ps1 -Language "zh"     # Chinese only
#   .\batch_transcription_test_with_naming.ps1 -GPU              # Use GPU acceleration

param(
    [string]$AudioDir = "Tests\Audio",
    [string]$ModelDir = "Tests\Models",
    [string]$OutputDir = "Tests\Results",
    [switch]$GPU = $false,
    [switch]$CleanResults = $true,
    [string]$Language = "auto"  # Options: "auto", "en", "zh", "ja", "ko", "fr", "de", etc.
)

Write-Host "=== Enhanced Batch Transcription Test ===" -ForegroundColor Cyan
Write-Host "Audio Directory: $AudioDir" -ForegroundColor Yellow
Write-Host "Model Directory: $ModelDir" -ForegroundColor Yellow
Write-Host "Output Directory: $OutputDir" -ForegroundColor Yellow
Write-Host "GPU Mode: $GPU" -ForegroundColor Yellow
Write-Host "Language Detection: $Language" -ForegroundColor Yellow
Write-Host "Clean Previous Results: $CleanResults" -ForegroundColor Yellow

# Create output directory if it doesn't exist
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    Write-Host "Created output directory: $OutputDir" -ForegroundColor Green
}

# Clean previous results if requested
if ($CleanResults) {
    Write-Host "Cleaning previous results..." -ForegroundColor Yellow
    Remove-Item "$OutputDir\*.txt" -Force -ErrorAction SilentlyContinue
    Remove-Item "$OutputDir\*.srt" -Force -ErrorAction SilentlyContinue
    Remove-Item "$OutputDir\*.vtt" -Force -ErrorAction SilentlyContinue
}

# Find audio files (support multiple formats)
$audioFiles = @()
$audioFiles += Get-ChildItem -Path $AudioDir -Filter "*.wav"
$audioFiles += Get-ChildItem -Path $AudioDir -Filter "*.mp3"
$audioFiles += Get-ChildItem -Path $AudioDir -Filter "*.wma"
$audioFiles = $audioFiles | Select-Object -First 3

if ($audioFiles.Count -eq 0) {
    Write-Host "No audio files found in $AudioDir" -ForegroundColor Red
    exit 1
}

# Find model files (use small models for reliable testing)
$modelFiles = Get-ChildItem -Path $ModelDir -Filter "ggml-small*.bin" | Select-Object -First 1
if ($modelFiles.Count -eq 0) {
    # Fallback to tiny models if no small models found
    $modelFiles = Get-ChildItem -Path $ModelDir -Filter "ggml-tiny*.bin" | Select-Object -First 1
}
if ($modelFiles.Count -eq 0) {
    Write-Host "No model files found in $ModelDir" -ForegroundColor Red
    exit 1
}

Write-Host "Found $($audioFiles.Count) audio files and $($modelFiles.Count) model files" -ForegroundColor Green

# Test results tracking
$results = @()
$totalTests = $audioFiles.Count * $modelFiles.Count
$currentTest = 0

# Custom naming pattern with performance info
$namingPattern = "{input}_{model}_{gpu}_perf_{time}"

foreach ($audioFile in $audioFiles) {
    foreach ($modelFile in $modelFiles) {
        $currentTest++
        $audioPath = $audioFile.FullName
        $modelPath = $modelFile.FullName
        $audioName = $audioFile.BaseName
        # Clean model name by removing file extension and replacing dots with underscores
        $modelName = $modelFile.BaseName -replace '\.', '_'
        
        Write-Host "`n[$currentTest/$totalTests] Testing: $audioName with $modelName" -ForegroundColor Cyan
        
        # Build command arguments (use no-timestamp mode to test our advanced sampling)
        $args = @(
            "-m", "`"$modelPath`""
            "-f", "`"$audioPath`""
            "-nt"
            "-otxt"
            "-op", "`"$OutputDir\$namingPattern`""
        )

        # Add language parameter if not auto
        if ($Language -ne "auto") {
            $args += @("-l", $Language)
        }
        
        if ($GPU) {
            $args += @("-gpu", "0")
        }
        
        # Execute transcription
        $startTime = Get-Date
        try {
            $output = & ".\Examples\main\x64\Release\main.exe" @args 2>&1
            $endTime = Get-Date
            $duration = ($endTime - $startTime).TotalSeconds
            
            # Parse performance info from output
            $cpuTime = "N/A"
            $gpuTime = "N/A"
            if ($output -match "RunComplete\s+([0-9.]+)\s+milliseconds") {
                $cpuTime = [math]::Round([double]$matches[1] / 1000, 2)
            }
            if ($output -match "Run\s+([0-9.]+)\s+milliseconds" -and $GPU) {
                $gpuTime = [math]::Round([double]$matches[1] / 1000, 2)
            }
            
            $result = [PSCustomObject]@{
                Audio = $audioName
                Model = $modelName
                Backend = if ($GPU) { "GPU" } else { "CPU" }
                Status = "SUCCESS"
                Duration = [math]::Round($duration, 2)
                CPUTime = $cpuTime
                GPUTime = $gpuTime
                Error = ""
            }
            
            Write-Host "  ✅ Success - Duration: $($duration)s, CPU: $($cpuTime)s" -ForegroundColor Green
            if ($GPU) {
                Write-Host "     GPU: $($gpuTime)s" -ForegroundColor Green
            }
            
        } catch {
            $endTime = Get-Date
            $duration = ($endTime - $startTime).TotalSeconds
            
            $result = [PSCustomObject]@{
                Audio = $audioName
                Model = $modelName
                Backend = if ($GPU) { "GPU" } else { "CPU" }
                Status = "FAILED"
                Duration = [math]::Round($duration, 2)
                CPUTime = "N/A"
                GPUTime = "N/A"
                Error = $_.Exception.Message
            }
            
            Write-Host "  ❌ Failed - Error: $($_.Exception.Message)" -ForegroundColor Red
        }
        
        $results += $result
    }
}

# Generate summary report
Write-Host "`n=== Test Summary ===" -ForegroundColor Cyan
$successCount = ($results | Where-Object { $_.Status -eq "SUCCESS" }).Count
$failCount = ($results | Where-Object { $_.Status -eq "FAILED" }).Count

Write-Host "Total Tests: $totalTests" -ForegroundColor Yellow
Write-Host "Successful: $successCount" -ForegroundColor Green
Write-Host "Failed: $failCount" -ForegroundColor Red

# Save detailed results
$reportPath = "$OutputDir\test_summary_$(Get-Date -Format 'yyyyMMdd_HHmmss').csv"
$results | Export-Csv -Path $reportPath -NoTypeInformation
Write-Host "Detailed results saved to: $reportPath" -ForegroundColor Green

# List generated files
Write-Host "`n=== Generated Files ===" -ForegroundColor Cyan
$generatedFiles = Get-ChildItem -Path $OutputDir -Filter "*.txt" | Sort-Object Name
foreach ($file in $generatedFiles) {
    $size = [math]::Round($file.Length / 1KB, 1)
    Write-Host "  $($file.Name) ($($size) KB)" -ForegroundColor White
}

Write-Host "`nBatch transcription test completed!" -ForegroundColor Green
