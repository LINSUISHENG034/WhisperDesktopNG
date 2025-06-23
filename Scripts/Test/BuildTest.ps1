# WhisperDesktopNG 编译测试脚本
# 自动化编译过程并验证结果

param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [switch]$Clean = $false
)

Write-Host "=== WhisperDesktopNG 编译测试 ===" -ForegroundColor Green
Write-Host "配置: $Configuration" -ForegroundColor Yellow
Write-Host "平台: $Platform" -ForegroundColor Yellow
Write-Host "测试时间: $(Get-Date)" -ForegroundColor Yellow

$ErrorActionPreference = "Stop"
$buildSuccess = $true

try {
    # 测试1: 检查编译环境
    Write-Host "`n[测试1] 检查编译环境..." -ForegroundColor Cyan
    
    # 检查MSBuild
    $msbuildPath = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($msbuildPath) {
        Write-Host "✅ MSBuild 可用: $($msbuildPath.Source)" -ForegroundColor Green
    } else {
        Write-Host "❌ MSBuild 不可用" -ForegroundColor Red
        $buildSuccess = $false
    }
    
    # 检查.NET
    $dotnetPath = Get-Command dotnet -ErrorAction SilentlyContinue
    if ($dotnetPath) {
        Write-Host "✅ .NET CLI 可用: $($dotnetPath.Source)" -ForegroundColor Green
    } else {
        Write-Host "❌ .NET CLI 不可用" -ForegroundColor Red
        $buildSuccess = $false
    }

    if (!$buildSuccess) {
        throw "编译环境检查失败"
    }

    # 测试2: 清理构建（如果需要）
    if ($Clean) {
        Write-Host "`n[测试2] 清理之前的构建..." -ForegroundColor Cyan
        if (Test-Path "x64") {
            Remove-Item -Path "x64" -Recurse -Force
            Write-Host "✅ 清理完成" -ForegroundColor Green
        } else {
            Write-Host "✅ 无需清理" -ForegroundColor Green
        }
    }

    # 测试3: 编译ComputeShaders
    Write-Host "`n[测试3] 编译ComputeShaders..." -ForegroundColor Cyan
    $result = & msbuild "ComputeShaders\ComputeShaders.vcxproj" /p:Configuration=$Configuration /p:Platform=$Platform /v:minimal
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ ComputeShaders 编译成功" -ForegroundColor Green
    } else {
        Write-Host "❌ ComputeShaders 编译失败" -ForegroundColor Red
        throw "ComputeShaders 编译失败"
    }

    # 测试4: 运行CompressShaders
    Write-Host "`n[测试4] 运行CompressShaders工具..." -ForegroundColor Cyan
    $compressResult = & dotnet run --project "Tools\CompressShaders\CompressShaders.csproj" --configuration $Configuration
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ CompressShaders 运行成功" -ForegroundColor Green
    } else {
        Write-Host "❌ CompressShaders 运行失败" -ForegroundColor Red
        throw "CompressShaders 运行失败"
    }

    # 测试5: 编译Whisper库
    Write-Host "`n[测试5] 编译Whisper库..." -ForegroundColor Cyan
    $result = & msbuild "Whisper\Whisper.vcxproj" /p:Configuration=$Configuration /p:Platform=$Platform /v:minimal
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Whisper库 编译成功" -ForegroundColor Green
    } else {
        Write-Host "❌ Whisper库 编译失败" -ForegroundColor Red
        throw "Whisper库 编译失败"
    }

    # 测试6: 编译WhisperDesktop应用程序
    Write-Host "`n[测试6] 编译WhisperDesktop应用程序..." -ForegroundColor Cyan
    $solutionDir = (Get-Location).Path + "\"
    $result = & msbuild "Examples\WhisperDesktop\WhisperDesktop.vcxproj" /p:Configuration=$Configuration /p:Platform=$Platform /p:SolutionDir=$solutionDir /v:minimal
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ WhisperDesktop 编译成功" -ForegroundColor Green
    } else {
        Write-Host "❌ WhisperDesktop 编译失败" -ForegroundColor Red
        throw "WhisperDesktop 编译失败"
    }

    # 测试7: 验证输出文件
    Write-Host "`n[测试7] 验证输出文件..." -ForegroundColor Cyan
    $outputPath = "x64\$Configuration"
    $requiredFiles = @("WhisperDesktop.exe", "Whisper.dll")
    
    foreach ($file in $requiredFiles) {
        $filePath = Join-Path $outputPath $file
        if (Test-Path $filePath) {
            $fileInfo = Get-Item $filePath
            Write-Host "✅ $file 存在 ($($fileInfo.Length) 字节)" -ForegroundColor Green
        } else {
            Write-Host "❌ $file 缺失" -ForegroundColor Red
            $buildSuccess = $false
        }
    }

    if ($buildSuccess) {
        Write-Host "`n=== 编译测试成功 ===" -ForegroundColor Green
        Write-Host "所有组件编译成功，输出文件完整。" -ForegroundColor Yellow
    } else {
        throw "输出文件验证失败"
    }

} catch {
    Write-Host "`n=== 编译测试失败 ===" -ForegroundColor Red
    Write-Host "错误: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
