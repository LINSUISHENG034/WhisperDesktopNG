# PowerShell script to build the CWhisperEngine encode/decode test
# This script compiles the test using the same build environment as the main project

param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

Write-Host "Building CWhisperEngine Encode/Decode Test" -ForegroundColor Green
Write-Host "Configuration: $Configuration" -ForegroundColor Yellow
Write-Host "Platform: $Platform" -ForegroundColor Yellow

# Set up paths
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$TestDir = $PSScriptRoot
$WhisperDir = Join-Path $ProjectRoot "Whisper"
$OutputDir = Join-Path $TestDir "bin" $Configuration $Platform

# Create output directory if it doesn't exist
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Compiler settings
$CppCompiler = "cl.exe"
$IncludePaths = @(
    "/I`"$WhisperDir`"",
    "/I`"$ProjectRoot`"",
    "/I`"$ProjectRoot\external\whisper.cpp\include`"",
    "/I`"$ProjectRoot\include`""
)

$SourceFiles = @(
    "`"$TestDir\CWhisperEngine_EncodeDecodeTest.cpp`"",
    "`"$WhisperDir\CWhisperEngine.cpp`""
)

$LibraryPaths = @(
    "/LIBPATH:`"$ProjectRoot\lib\$Platform\$Configuration`"",
    "/LIBPATH:`"$ProjectRoot\external\whisper.cpp\lib\$Platform`""
)

$Libraries = @(
    "whisper.lib",
    "kernel32.lib",
    "user32.lib"
)

$CompilerFlags = @(
    "/EHsc",           # Enable C++ exception handling
    "/std:c++17",      # Use C++17 standard
    "/W3",             # Warning level 3
    "/nologo",         # Suppress startup banner
    "/MD"              # Use multithreaded DLL runtime
)

if ($Configuration -eq "Debug") {
    $CompilerFlags += @("/Od", "/Zi", "/DEBUG")  # Debug optimizations and debug info
} else {
    $CompilerFlags += @("/O2", "/DNDEBUG")       # Release optimizations
}

$LinkerFlags = @(
    "/nologo",
    "/SUBSYSTEM:CONSOLE"
)

# Build command
$BuildCommand = @(
    $CppCompiler
    $CompilerFlags
    $IncludePaths
    $SourceFiles
    "/Fe:`"$OutputDir\CWhisperEngine_EncodeDecodeTest.exe`""
    "/link"
    $LinkerFlags
    $LibraryPaths
    $Libraries
) -join " "

Write-Host "Build command:" -ForegroundColor Cyan
Write-Host $BuildCommand -ForegroundColor Gray

# Execute build
try {
    Write-Host "Compiling..." -ForegroundColor Yellow
    Invoke-Expression $BuildCommand
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Build successful!" -ForegroundColor Green
        Write-Host "Test executable: $OutputDir\CWhisperEngine_EncodeDecodeTest.exe" -ForegroundColor Green
        
        # Check if executable was created
        $ExePath = Join-Path $OutputDir "CWhisperEngine_EncodeDecodeTest.exe"
        if (Test-Path $ExePath) {
            Write-Host "Executable size: $((Get-Item $ExePath).Length) bytes" -ForegroundColor Gray
        }
    } else {
        Write-Host "Build failed with exit code: $LASTEXITCODE" -ForegroundColor Red
        exit $LASTEXITCODE
    }
} catch {
    Write-Host "Build error: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "To run the test, use:" -ForegroundColor Cyan
Write-Host "$OutputDir\CWhisperEngine_EncodeDecodeTest.exe <model_path>" -ForegroundColor Gray
Write-Host "Example:" -ForegroundColor Cyan
Write-Host "$OutputDir\CWhisperEngine_EncodeDecodeTest.exe models\ggml-base.en.bin" -ForegroundColor Gray
