# WhisperDesktopNG 统一构建和同步脚本
# 确保所有组件都是最新版本并在同一目录中

param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

Write-Host "=== WhisperDesktopNG 统一构建脚本 ===" -ForegroundColor Green
Write-Host "配置: $Configuration, 平台: $Platform" -ForegroundColor Yellow

# 设置路径
$SolutionPath = "WhisperDesktopNG.sln"
$OutputDir = "x64\$Configuration"
$WhisperDllSource = "Whisper\x64\$Configuration\Whisper.dll"
$WhisperPdbSource = "Whisper\x64\$Configuration\Whisper.pdb"

# 步骤1: 清理旧文件
Write-Host "`n步骤1: 清理旧的输出文件..." -ForegroundColor Cyan
if (Test-Path $OutputDir) {
    Remove-Item "$OutputDir\*.dll" -Force -ErrorAction SilentlyContinue
    Remove-Item "$OutputDir\*.pdb" -Force -ErrorAction SilentlyContinue
    Remove-Item "$OutputDir\*.exe" -Force -ErrorAction SilentlyContinue
    Write-Host "清理完成" -ForegroundColor Green
}

# 步骤2: 完全重建整个解决方案
Write-Host "`n步骤2: 重建整个解决方案..." -ForegroundColor Cyan
$buildCmd = "msbuild `"$SolutionPath`" /p:Configuration=$Configuration /p:Platform=$Platform /t:Rebuild /m"
Write-Host "执行命令: $buildCmd" -ForegroundColor Gray

$buildResult = Invoke-Expression $buildCmd
$buildExitCode = $LASTEXITCODE

if ($buildExitCode -ne 0) {
    Write-Host "构建失败，退出代码: $buildExitCode" -ForegroundColor Red
    exit $buildExitCode
}

Write-Host "解决方案构建成功!" -ForegroundColor Green

# 步骤3: 验证关键文件存在
Write-Host "`n步骤3: 验证关键文件..." -ForegroundColor Cyan

$requiredFiles = @(
    "$OutputDir\main.exe",
    "$WhisperDllSource"
)

foreach ($file in $requiredFiles) {
    if (Test-Path $file) {
        $fileInfo = Get-Item $file
        Write-Host "✓ $file (大小: $($fileInfo.Length) 字节, 修改时间: $($fileInfo.LastWriteTime))" -ForegroundColor Green
    } else {
        Write-Host "✗ 缺失文件: $file" -ForegroundColor Red
        exit 1
    }
}

# 步骤4: 同步DLL文件到输出目录
Write-Host "`n步骤4: 同步DLL文件..." -ForegroundColor Cyan

# 复制Whisper.dll
if (Test-Path $WhisperDllSource) {
    Copy-Item $WhisperDllSource $OutputDir -Force
    Write-Host "✓ 复制 Whisper.dll" -ForegroundColor Green
} else {
    Write-Host "✗ 源文件不存在: $WhisperDllSource" -ForegroundColor Red
    exit 1
}

# 复制Whisper.pdb (调试符号)
if (Test-Path $WhisperPdbSource) {
    Copy-Item $WhisperPdbSource $OutputDir -Force
    Write-Host "✓ 复制 Whisper.pdb" -ForegroundColor Green
}

# 步骤5: 验证最终文件
Write-Host "`n步骤5: 验证最终输出..." -ForegroundColor Cyan

$finalFiles = @(
    "$OutputDir\main.exe",
    "$OutputDir\Whisper.dll"
)

foreach ($file in $finalFiles) {
    if (Test-Path $file) {
        $fileInfo = Get-Item $file
        Write-Host "✓ $file (修改时间: $($fileInfo.LastWriteTime))" -ForegroundColor Green
    } else {
        Write-Host "✗ 最终文件缺失: $file" -ForegroundColor Red
        exit 1
    }
}

Write-Host "`n=== 构建和同步完成! ===" -ForegroundColor Green
Write-Host "所有文件已准备就绪，可以运行测试。" -ForegroundColor Yellow
Write-Host "运行命令: .\$OutputDir\main.exe -m `"E:\Program Files\WhisperDesktop\ggml-tiny.bin`" -f `"SampleClips\jfk.wav`"" -ForegroundColor Gray
