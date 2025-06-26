# WhisperCpp PCM转录自动化测试脚本
# 用法: .\Scripts\test_whisper_cpp_pcm.ps1

param(
    [string]$ModelPath = "E:\Program Files\WhisperDesktop\ggml-tiny.bin",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

Write-Host "=== WhisperCpp PCM转录自动化测试 ===" -ForegroundColor Green
Write-Host "模型路径: $ModelPath" -ForegroundColor Yellow
Write-Host "编译配置: $Configuration" -ForegroundColor Yellow
Write-Host "平台: $Platform" -ForegroundColor Yellow
Write-Host ""

# 检查项目根目录
$ProjectRoot = "F:\Projects\WhisperDesktopNG"
if (-not (Test-Path $ProjectRoot)) {
    Write-Error "项目根目录不存在: $ProjectRoot"
    exit 1
}

Set-Location $ProjectRoot

# 检查模型文件
if (-not (Test-Path $ModelPath)) {
    Write-Error "模型文件不存在: $ModelPath"
    exit 1
}

Write-Host "1. 编译Whisper.dll..." -ForegroundColor Cyan
$result = & msbuild "Whisper\Whisper.vcxproj" /p:Configuration=$Configuration /p:Platform=$Platform /nologo
if ($LASTEXITCODE -ne 0) {
    Write-Error "Whisper.dll编译失败"
    exit 1
}
Write-Host "   ✅ Whisper.dll编译成功" -ForegroundColor Green

Write-Host "2. 编译main.exe..." -ForegroundColor Cyan
$result = & msbuild "Examples\main\main.vcxproj" /p:Configuration=$Configuration /p:Platform=$Platform /nologo
if ($LASTEXITCODE -ne 0) {
    Write-Error "main.exe编译失败"
    exit 1
}
Write-Host "   ✅ main.exe编译成功" -ForegroundColor Green

Write-Host "3. 复制DLL文件..." -ForegroundColor Cyan
$dllSource = "Whisper\$Platform\$Configuration\Whisper.dll"
$dllTarget = "Examples\main\$Platform\$Configuration\"
Copy-Item $dllSource $dllTarget -Force
Write-Host "   ✅ DLL复制完成" -ForegroundColor Green

# 测试音频文件列表
$testAudios = @(
    @{
        Name = "JFK演讲 (短音频)"
        Path = "SampleClips\jfk.wav"
        ExpectedDuration = 11
    }
    @{
        Name = "Columbia悼词 (长音频)"
        Path = "SampleClips\columbia_converted.wav"
        ExpectedDuration = 198
    }
)

$testResults = @()

foreach ($audio in $testAudios) {
    Write-Host ""
    Write-Host "4. 测试: $($audio.Name)" -ForegroundColor Cyan
    
    # 检查音频文件
    if (-not (Test-Path $audio.Path)) {
        Write-Warning "音频文件不存在: $($audio.Path)"
        continue
    }
    
    # 运行测试
    $testExe = "Examples\main\$Platform\$Configuration\main.exe"
    $startTime = Get-Date
    
    Write-Host "   执行: $testExe --test-pcm `"$ModelPath`" `"$($audio.Path)`"" -ForegroundColor Gray
    
    $output = & $testExe --test-pcm $ModelPath $audio.Path 2>&1
    $exitCode = $LASTEXITCODE
    $endTime = Get-Date
    $duration = ($endTime - $startTime).TotalMilliseconds
    
    # 解析结果
    $success = $exitCode -eq 0
    $segments = 0
    $confidence = 0.0
    $transcriptText = ""
    
    if ($success) {
        # 提取分段数
        $segmentMatch = $output | Select-String "Number of segments: (\d+)"
        if ($segmentMatch) {
            $segments = [int]$segmentMatch.Matches[0].Groups[1].Value
        }
        
        # 提取第一个分段的置信度
        $confidenceMatch = $output | Select-String "Confidence: ([\d.]+)"
        if ($confidenceMatch) {
            $confidence = [float]$confidenceMatch.Matches[0].Groups[1].Value
        }
        
        # 提取第一个分段的文本
        $textMatch = $output | Select-String 'Text: "(.*)"'
        if ($textMatch) {
            $transcriptText = $textMatch.Matches[0].Groups[1].Value
        }
    }
    
    # 记录结果
    $testResult = @{
        AudioName = $audio.Name
        AudioPath = $audio.Path
        Success = $success
        ExitCode = $exitCode
        Duration = $duration
        Segments = $segments
        Confidence = $confidence
        TranscriptText = $transcriptText
        Output = ($output -join "`n")
    }
    $testResults += $testResult
    
    # 显示结果
    if ($success) {
        Write-Host "   ✅ 测试成功" -ForegroundColor Green
        Write-Host "      分段数: $segments" -ForegroundColor White
        Write-Host "      置信度: $confidence" -ForegroundColor White
        Write-Host "      耗时: $([math]::Round($duration, 2))ms" -ForegroundColor White
        Write-Host "      文本预览: $($transcriptText.Substring(0, [math]::Min(50, $transcriptText.Length)))..." -ForegroundColor White
    } else {
        Write-Host "   ❌ 测试失败 (退出码: $exitCode)" -ForegroundColor Red
    }
}

# 生成测试报告
Write-Host ""
Write-Host "=== 测试报告 ===" -ForegroundColor Green

$successCount = ($testResults | Where-Object { $_.Success }).Count
$totalCount = $testResults.Count

Write-Host "总测试数: $totalCount" -ForegroundColor White
Write-Host "成功数: $successCount" -ForegroundColor Green
Write-Host "失败数: $($totalCount - $successCount)" -ForegroundColor Red
Write-Host "成功率: $([math]::Round($successCount / $totalCount * 100, 1))%" -ForegroundColor Yellow

# 详细结果
foreach ($result in $testResults) {
    Write-Host ""
    Write-Host "--- $($result.AudioName) ---" -ForegroundColor Cyan
    Write-Host "状态: $(if ($result.Success) { '✅ 成功' } else { '❌ 失败' })"
    Write-Host "耗时: $([math]::Round($result.Duration, 2))ms"
    Write-Host "分段数: $($result.Segments)"
    Write-Host "置信度: $($result.Confidence)"
    if ($result.TranscriptText) {
        Write-Host "转录文本: $($result.TranscriptText)"
    }
}

# 保存详细日志
$logFile = "Tests\whisper_cpp_pcm_test_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"
$logDir = Split-Path $logFile -Parent
if (-not (Test-Path $logDir)) {
    New-Item -ItemType Directory -Path $logDir -Force | Out-Null
}

$logContent = @"
WhisperCpp PCM转录测试报告
生成时间: $(Get-Date)
模型路径: $ModelPath
编译配置: $Configuration $Platform

测试结果汇总:
总测试数: $totalCount
成功数: $successCount
失败数: $($totalCount - $successCount)
成功率: $([math]::Round($successCount / $totalCount * 100, 1))%

详细结果:
$($testResults | ForEach-Object {
    "
=== $($_.AudioName) ===
音频路径: $($_.AudioPath)
测试状态: $(if ($_.Success) { '成功' } else { '失败' })
退出码: $($_.ExitCode)
执行时间: $([math]::Round($_.Duration, 2))ms
分段数量: $($_.Segments)
置信度: $($_.Confidence)
转录文本: $($_.TranscriptText)

完整输出:
$($_.Output)
"
} | Out-String)
"@

$logContent | Out-File -FilePath $logFile -Encoding UTF8
Write-Host ""
Write-Host "详细日志已保存到: $logFile" -ForegroundColor Yellow

# 返回结果
if ($successCount -eq $totalCount) {
    Write-Host ""
    Write-Host "🎉 所有测试通过！WhisperCpp PCM转录功能正常工作。" -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "⚠️  部分测试失败，请检查详细日志。" -ForegroundColor Yellow
    exit 1
}
