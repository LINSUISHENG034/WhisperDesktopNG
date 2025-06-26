# 简化的WhisperCpp PCM转录测试脚本

Write-Host "=== WhisperCpp PCM转录测试 ===" -ForegroundColor Green

$ProjectRoot = "F:\Projects\WhisperDesktopNG"
$ModelPath = "E:\Program Files\WhisperDesktop\ggml-tiny.bin"

Set-Location $ProjectRoot

Write-Host "1. 编译项目..." -ForegroundColor Cyan
& msbuild "Whisper\Whisper.vcxproj" /p:Configuration=Debug /p:Platform=x64 /nologo
if ($LASTEXITCODE -ne 0) {
    Write-Error "编译失败"
    exit 1
}

& msbuild "Examples\main\main.vcxproj" /p:Configuration=Debug /p:Platform=x64 /nologo
if ($LASTEXITCODE -ne 0) {
    Write-Error "编译失败"
    exit 1
}

Write-Host "2. 复制DLL..." -ForegroundColor Cyan
Copy-Item "Whisper\x64\Debug\Whisper.dll" "Examples\main\x64\Debug\" -Force

Write-Host "3. 测试JFK音频..." -ForegroundColor Cyan
Set-Location "Examples\main\x64\Debug"
$output1 = & .\main.exe --test-pcm $ModelPath "F:\Projects\WhisperDesktopNG\SampleClips\jfk.wav"
$result1 = $LASTEXITCODE

Write-Host "4. 测试Columbia音频..." -ForegroundColor Cyan
$output2 = & .\main.exe --test-pcm $ModelPath "F:\Projects\WhisperDesktopNG\SampleClips\columbia_converted.wav"
$result2 = $LASTEXITCODE

Write-Host ""
Write-Host "=== 测试结果 ===" -ForegroundColor Green
Write-Host "JFK音频: $(if ($result1 -eq 0) { '✅ 成功' } else { '❌ 失败' })" -ForegroundColor $(if ($result1 -eq 0) { 'Green' } else { 'Red' })
Write-Host "Columbia音频: $(if ($result2 -eq 0) { '✅ 成功' } else { '❌ 失败' })" -ForegroundColor $(if ($result2 -eq 0) { 'Green' } else { 'Red' })

if ($result1 -eq 0 -and $result2 -eq 0) {
    Write-Host ""
    Write-Host "🎉 所有测试通过！" -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "⚠️ 部分测试失败" -ForegroundColor Yellow
    exit 1
}
