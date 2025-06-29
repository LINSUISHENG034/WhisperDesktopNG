# Phase2 QA Batch Transcription Test Script
# 自动转录Tests/Audio目录下的所有音频文件

param(
    [string]$ModelPath = "Tests\Models\ggml-large-v3.bin",
    [string]$AudioDir = "Tests\Audio",
    [string]$Language = "en"
)

Write-Host "=== Phase2 QA Batch Transcription Test ===" -ForegroundColor Green
Write-Host "Model: $ModelPath" -ForegroundColor Cyan
Write-Host "Audio Directory: $AudioDir" -ForegroundColor Cyan
Write-Host "Language: $Language" -ForegroundColor Cyan
Write-Host ""

# 检查main.exe是否存在
$mainExe = "Examples\main\x64\Release\main.exe"
if (-not (Test-Path $mainExe)) {
    Write-Host "[ERROR]: main.exe not found at $mainExe" -ForegroundColor Red
    Write-Host "Please compile the main project first:" -ForegroundColor Yellow
    Write-Host "  msbuild Examples\main\main.vcxproj /p:Configuration=Release /p:Platform=x64" -ForegroundColor Yellow
    exit 1
}

# 检查模型文件是否存在
if (-not (Test-Path $ModelPath)) {
    Write-Host "[ERROR]: Model file not found at $ModelPath" -ForegroundColor Red
    exit 1
}

# 检查音频目录是否存在
if (-not (Test-Path $AudioDir)) {
    Write-Host "[ERROR]: Audio directory not found at $AudioDir" -ForegroundColor Red
    exit 1
}

# 获取所有音频文件
$audioExtensions = @("*.wav", "*.mp3", "*.wma", "*.m4a", "*.flac")
$audioFiles = @()

foreach ($ext in $audioExtensions) {
    $files = Get-ChildItem -Path $AudioDir -Filter $ext -File
    $audioFiles += $files
}

if ($audioFiles.Count -eq 0) {
    Write-Host "[WARNING]: No audio files found in $AudioDir" -ForegroundColor Yellow
    exit 0
}

Write-Host "Found $($audioFiles.Count) audio files:" -ForegroundColor Green
foreach ($file in $audioFiles) {
    $sizeKB = [math]::Round($file.Length / 1024, 1)
    Write-Host "  - $($file.Name) ($sizeKB KB)" -ForegroundColor White
}
Write-Host ""

# 开始转录
$successCount = 0
$failCount = 0
$totalStartTime = Get-Date

foreach ($audioFile in $audioFiles) {
    $fileName = $audioFile.Name
    $filePath = $audioFile.FullName
    $outputFile = "$($filePath).txt"
    
    Write-Host "Processing: $fileName" -ForegroundColor Yellow
    
    # 如果输出文件已存在，先删除
    if (Test-Path $outputFile) {
        Remove-Item $outputFile -Force
    }
    
    $startTime = Get-Date
    
    try {
        # 执行转录
        $result = & $mainExe -m $ModelPath -f $filePath -l $Language -otxt 2>&1
        $exitCode = $LASTEXITCODE
        
        $endTime = Get-Date
        $duration = ($endTime - $startTime).TotalSeconds
        
        if ($exitCode -eq 0 -and (Test-Path $outputFile)) {
            # 检查输出文件内容
            $content = Get-Content $outputFile -Raw
            if ($content -and $content.Trim().Length -gt 0) {
                $successCount++
                Write-Host "  ✅ SUCCESS ($([math]::Round($duration, 1))s)" -ForegroundColor Green
                Write-Host "     Output: $($content.Trim())" -ForegroundColor Gray
            } else {
                $failCount++
                Write-Host "  ❌ FAIL: Empty output file ($([math]::Round($duration, 1))s)" -ForegroundColor Red
            }
        } else {
            $failCount++
            Write-Host "  ❌ FAIL: Exit code $exitCode ($([math]::Round($duration, 1))s)" -ForegroundColor Red
            if ($result) {
                Write-Host "     Error: $result" -ForegroundColor Red
            }
        }
    } catch {
        $failCount++
        $endTime = Get-Date
        $duration = ($endTime - $startTime).TotalSeconds
        Write-Host "  ❌ FAIL: Exception ($([math]::Round($duration, 1))s)" -ForegroundColor Red
        Write-Host "     Error: $($_.Exception.Message)" -ForegroundColor Red
    }
    
    Write-Host ""
}

$totalEndTime = Get-Date
$totalDuration = ($totalEndTime - $totalStartTime).TotalSeconds

# 生成总结报告
Write-Host "=== Batch Transcription Summary ===" -ForegroundColor Green
Write-Host "Total Files: $($audioFiles.Count)" -ForegroundColor White
Write-Host "Successful: $successCount" -ForegroundColor Green
Write-Host "Failed: $failCount" -ForegroundColor Red
Write-Host "Success Rate: $([math]::Round($successCount * 100 / $audioFiles.Count, 1))%" -ForegroundColor Cyan
Write-Host "Total Time: $([math]::Round($totalDuration, 1)) seconds" -ForegroundColor Cyan
Write-Host ""

# 列出生成的文件
Write-Host "Generated transcription files:" -ForegroundColor Green
$transcriptionFiles = Get-ChildItem -Path $AudioDir -Filter "*.txt" -File
foreach ($file in $transcriptionFiles) {
    $sizeBytes = $file.Length
    if ($sizeBytes -gt 0) {
        Write-Host "  ✅ $($file.Name) ($sizeBytes bytes)" -ForegroundColor Green
    } else {
        Write-Host "  ❌ $($file.Name) (empty)" -ForegroundColor Red
    }
}

if ($failCount -eq 0) {
    Write-Host ""
    Write-Host "🎉 All transcriptions completed successfully!" -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "⚠️  Some transcriptions failed. Please check the errors above." -ForegroundColor Yellow
    exit 1
}
