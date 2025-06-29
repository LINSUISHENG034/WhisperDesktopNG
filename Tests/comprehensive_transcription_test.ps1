# Comprehensive Transcription Test Script
# Tests multiple models against multiple audio files
# Automatically cleans previous results and generates detailed reports

param(
    [string]$TestMode = "full",  # "full", "quick", "debug"
    [switch]$CleanOnly = $false
)

# Configuration
$ProjectRoot = "F:\Projects\WhisperDesktopNG"
$MainExe = "$ProjectRoot\Examples\main\x64\Release\main.exe"
$AudioDir = "$ProjectRoot\Tests\Audio"
$ModelsDir = "$ProjectRoot\Tests\Models"
$ResultsDir = "$ProjectRoot\Tests\TranscriptionResults"
$LogFile = "$ProjectRoot\Tests\transcription_test_log.txt"

# Test configurations
$Models = @(
    @{Name="base-q5_1"; File="ggml-base-q5_1.bin"; Description="Base Q5_1 Quantized"},
    @{Name="large-v3"; File="ggml-large-v3.bin"; Description="Large-v3 Full Precision"}
)

$AudioFiles = @(
    @{Name="jfk"; File="jfk.wav"; Language="en"; Description="Short English (11s)"},
    @{Name="columbia"; File="columbia.wma"; Language="zh"; Description="Chinese Audio (33min)"},
    @{Name="long_audio"; File="long_audio_7517965755242105650.mp3"; Language="en"; Description="Long English (81min)"},
    @{Name="medium_audio"; File="medium_audio_7509110973165226778.mp3"; Language="en"; Description="Medium English"},
    @{Name="short_audio"; File="short_audio_7517965755242105650.mp3"; Language="en"; Description="Short English"}
)

# Quick test subset for debugging
$QuickModels = @($Models[0])  # Only base-q5_1
$QuickAudio = @($AudioFiles[0])  # Only jfk.wav

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logEntry = "[$timestamp] [$Level] $Message"
    Write-Host $logEntry
    Add-Content -Path $LogFile -Value $logEntry
}

function Clean-PreviousResults {
    Write-Log "Cleaning previous test results..." "INFO"

    # Clean .txt files in Audio directory (legacy location)
    $txtFiles = Get-ChildItem "$AudioDir\*.txt" -ErrorAction SilentlyContinue
    if ($txtFiles) {
        $txtFiles | Remove-Item -Force
        Write-Log "Removed $($txtFiles.Count) .txt files from Audio directory" "INFO"
    }

    # Clean and recreate results directory
    if (Test-Path $ResultsDir) {
        Remove-Item $ResultsDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $ResultsDir -Force | Out-Null
    Write-Log "Results directory cleaned and recreated" "INFO"

    # Also clean any .txt files in project root (from previous tests)
    $rootTxtFiles = Get-ChildItem "$ProjectRoot\*.txt" -ErrorAction SilentlyContinue
    if ($rootTxtFiles) {
        $rootTxtFiles | Remove-Item -Force
        Write-Log "Removed $($rootTxtFiles.Count) .txt files from project root" "INFO"
    }
}

function Test-Prerequisites {
    Write-Log "Checking test prerequisites..." "INFO"
    
    $errors = @()
    
    if (!(Test-Path $MainExe)) {
        $errors += "Main executable not found: $MainExe"
    }
    
    if (!(Test-Path $AudioDir)) {
        $errors += "Audio directory not found: $AudioDir"
    }
    
    if (!(Test-Path $ModelsDir)) {
        $errors += "Models directory not found: $ModelsDir"
    }
    
    if ($errors.Count -gt 0) {
        Write-Log "Prerequisites check failed:" "ERROR"
        $errors | ForEach-Object { Write-Log "  - $_" "ERROR" }
        return $false
    }
    
    Write-Log "All prerequisites satisfied" "SUCCESS"
    return $true
}

function Get-AudioDuration {
    param([string]$AudioPath)
    # Simple duration estimation based on file size (rough approximation)
    $fileSize = (Get-Item $AudioPath).Length
    if ($fileSize -lt 1MB) { return "Short (<1min)" }
    elseif ($fileSize -lt 10MB) { return "Medium (1-10min)" }
    elseif ($fileSize -lt 100MB) { return "Long (10-60min)" }
    else { return "Very Long (>60min)" }
}

function Run-TranscriptionTest {
    param(
        [hashtable]$Model,
        [hashtable]$Audio,
        [string]$OutputDir
    )
    
    $modelPath = "$ModelsDir\$($Model.File)"
    $audioPath = "$AudioDir\$($Audio.File)"
    $outputFile = "$OutputDir\$($Model.Name)_$($Audio.Name).txt"
    
    # Check if files exist
    if (!(Test-Path $modelPath)) {
        Write-Log "Model not found: $modelPath" "ERROR"
        return @{Success=$false; Error="Model file not found"}
    }
    
    if (!(Test-Path $audioPath)) {
        Write-Log "Audio not found: $audioPath" "ERROR"
        return @{Success=$false; Error="Audio file not found"}
    }
    
    Write-Log "Testing: $($Model.Description) + $($Audio.Description)" "INFO"
    
    # Build command with custom naming pattern to avoid file conflicts
    $namingPattern = "$OutputDir\{input}_{model}_cpu_{time}"
    $cmd = "& `"$MainExe`" -m `"$modelPath`" -f `"$audioPath`" -l $($Audio.Language) -otxt -nt -op `"$namingPattern`""

    Write-Log "Command: $cmd" "DEBUG"
    
    # Measure execution time
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    try {
        # Execute transcription
        $output = Invoke-Expression $cmd 2>&1
        $exitCode = $LASTEXITCODE
        
        $stopwatch.Stop()
        $duration = $stopwatch.Elapsed.TotalSeconds
        
        # Check for generated output file with new naming pattern
        # Find the most recently created file matching our pattern
        $audioBaseName = [System.IO.Path]::GetFileNameWithoutExtension($Audio.File)
        $modelBaseName = [System.IO.Path]::GetFileNameWithoutExtension($Model.File)
        $generatedFiles = Get-ChildItem "$OutputDir\${audioBaseName}_${modelBaseName}_cpu_*.txt" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending

        $transcriptionContent = ""
        $hasValidTranscription = $false
        $generatedTxt = ""

        if ($generatedFiles -and $generatedFiles.Count -gt 0) {
            $generatedTxt = $generatedFiles[0].FullName
            $transcriptionContent = Get-Content $generatedTxt -Raw
            $hasValidTranscription = $transcriptionContent -and
                                   $transcriptionContent.Trim() -ne "" -and
                                   $transcriptionContent -notmatch "Timestamp generation failed"

            Write-Log "Found generated file: $($generatedFiles[0].Name)" "INFO"
        }
        
        # Parse performance metrics from output
        $loadTime = if ($output -match "LoadModel\s+([0-9.]+)\s+(seconds|milliseconds)") { 
            if ($matches[2] -eq "milliseconds") { [float]$matches[1] / 1000 } else { [float]$matches[1] }
        } else { "N/A" }
        
        $runTime = if ($output -match "RunComplete\s+([0-9.]+)\s+seconds") { [float]$matches[1] } else { "N/A" }
        $vramUsage = if ($output -match "Total\s+[0-9.]+\s+[A-Z]+\s+RAM,\s+([0-9.]+)\s+([A-Z]+)\s+VRAM") { "$($matches[1]) $($matches[2])" } else { "N/A" }
        
        return @{
            Success = $exitCode -eq 0
            Duration = $duration
            LoadTime = $loadTime
            RunTime = $runTime
            VramUsage = $vramUsage
            HasValidTranscription = $hasValidTranscription
            TranscriptionLength = $transcriptionContent.Length
            TranscriptionPreview = $transcriptionContent.Substring(0, [Math]::Min(100, $transcriptionContent.Length))
            Output = $output -join "`n"
            ExitCode = $exitCode
        }
    }
    catch {
        $stopwatch.Stop()
        Write-Log "Test execution failed: $($_.Exception.Message)" "ERROR"
        return @{
            Success = $false
            Error = $_.Exception.Message
            Duration = $stopwatch.Elapsed.TotalSeconds
        }
    }
}

function Generate-TestReport {
    param([array]$Results)
    
    $reportPath = "$ResultsDir\test_report.md"
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    
    $report = @"
# Comprehensive Transcription Test Report

**Generated**: $timestamp  
**Test Mode**: $TestMode  
**Total Tests**: $($Results.Count)

## Executive Summary

"@

    # Calculate summary statistics
    $successfulTests = $Results | Where-Object { $_.Success }
    $failedTests = $Results | Where-Object { !$_.Success }
    $validTranscriptions = $Results | Where-Object { $_.HasValidTranscription }
    
    $report += @"

- **Successful Executions**: $($successfulTests.Count)/$($Results.Count) ($([math]::Round($successfulTests.Count/$Results.Count*100, 1))%)
- **Valid Transcriptions**: $($validTranscriptions.Count)/$($Results.Count) ($([math]::Round($validTranscriptions.Count/$Results.Count*100, 1))%)
- **Critical Issue**: $($Results.Count - $validTranscriptions.Count) tests produced no valid transcription text

## Detailed Results

| Model | Audio | Success | Load Time | Run Time | VRAM | Valid Text | Status |
|-------|-------|---------|-----------|----------|------|------------|--------|
"@

    foreach ($result in $Results) {
        $status = if ($result.HasValidTranscription) { "✅ PASS" } 
                 elseif ($result.Success) { "⚠️ NO TEXT" } 
                 else { "❌ FAIL" }
        
        $report += "`n| $($result.Model) | $($result.Audio) | $($result.Success) | $($result.LoadTime) | $($result.RunTime) | $($result.VramUsage) | $($result.HasValidTranscription) | $status |"
    }
    
    $report += @"


## Critical Findings

### Transcription Failure Pattern
$($Results.Count - $validTranscriptions.Count) out of $($Results.Count) tests failed to generate valid transcription text, showing consistent pattern:
- Timestamp generation fails after 5 attempts
- Output contains only error message: "[Timestamp generation failed - remaining audio processed without timestamps]"
- Performance metrics are excellent but core functionality is broken

### Performance Analysis
"@

    if ($successfulTests.Count -gt 0) {
        $avgLoadTime = ($successfulTests | Where-Object { $_.LoadTime -ne "N/A" } | Measure-Object LoadTime -Average).Average
        $avgRunTime = ($successfulTests | Where-Object { $_.RunTime -ne "N/A" } | Measure-Object RunTime -Average).Average
        
        $report += @"
- **Average Load Time**: $([math]::Round($avgLoadTime, 2)) seconds
- **Average Run Time**: $([math]::Round($avgRunTime, 2)) seconds
- **Performance**: Excellent (processing much faster than real-time)
"@
    }
    
    $report += @"

## Recommendations

1. **CRITICAL**: Debug timestamp generation logic in runFullImpl
2. **HIGH**: Investigate token-to-text conversion pipeline  
3. **MEDIUM**: Test with original non-quantized models for comparison
4. **LOW**: Performance optimization (already excellent)

---
*Report generated by comprehensive_transcription_test.ps1*
"@

    Set-Content -Path $reportPath -Value $report
    Write-Log "Test report generated: $reportPath" "SUCCESS"
}

# Main execution
function Main {
    Write-Log "=== Comprehensive Transcription Test Started ===" "INFO"
    Write-Log "Test Mode: $TestMode" "INFO"
    
    if ($CleanOnly) {
        Clean-PreviousResults
        Write-Log "Clean-only mode completed" "INFO"
        return
    }
    
    if (!(Test-Prerequisites)) {
        Write-Log "Prerequisites check failed. Exiting." "ERROR"
        exit 1
    }
    
    Clean-PreviousResults
    
    # Select test sets based on mode
    $testModels = if ($TestMode -eq "quick") { $QuickModels } else { $Models }
    $testAudio = if ($TestMode -eq "quick") { $QuickAudio } else { $AudioFiles }
    
    Write-Log "Testing $($testModels.Count) models against $($testAudio.Count) audio files" "INFO"

    $results = @()
    $testCount = 0
    $totalTests = $testModels.Count * $testAudio.Count

    foreach ($model in $testModels) {
        foreach ($audio in $testAudio) {
            $testCount++
            Write-Log "[$testCount/$totalTests] Testing $($model.Name) + $($audio.Name)" "INFO"

            $result = Run-TranscriptionTest -Model $model -Audio $audio -OutputDir $ResultsDir
            # Add metadata to result
            $result = $result + @{
                Model = $model.Name
                Audio = $audio.Name
            }
            $results += $result
            
            if ($result.Success) {
                if ($result.HasValidTranscription) {
                    Write-Log "✅ SUCCESS: Valid transcription generated" "SUCCESS"
                } else {
                    Write-Log "⚠️ WARNING: No valid transcription text" "WARNING"
                }
            } else {
                Write-Log "❌ FAILED: $($result.Error)" "ERROR"
            }
        }
    }
    
    Generate-TestReport -Results $results
    
    Write-Log "=== Test Completed ===" "INFO"
    Write-Log "Results saved to: $ResultsDir" "INFO"
    Write-Log "Log file: $LogFile" "INFO"
    
    # Summary
    $validCount = ($results | Where-Object { $_.HasValidTranscription }).Count
    Write-Log "SUMMARY: $validCount/$($results.Count) tests produced valid transcriptions" $(if ($validCount -eq 0) { "ERROR" } else { "INFO" })
}

# Execute main function
Main
