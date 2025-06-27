# Tests Directory

This directory contains all testing and validation components for the WhisperDesktopNG project.

## Directory Structure

### Core Test Projects

- **`GGML/`** - Basic GGML functionality tests
  - `TestGGML.vcxproj` - Visual Studio project for GGML integration testing
  - `main.cpp` - Basic model loading and API verification tests

- **`QuantizedModels/`** - Quantized model compatibility tests
  - `TestQuantizedModels.vcxproj` - Visual Studio project for quantization testing
  - `main.cpp` - Comprehensive quantized model loading tests

- **`PerformanceBenchmark/`** - Performance analysis and benchmarking
  - `PerformanceBenchmark.vcxproj` - Visual Studio project for performance testing
  - `main.cpp` - Systematic performance benchmarking suite

### Test Data

- **`Models/`** - Test model files (excluded from Git)
  - Various quantized Whisper models for testing
  - File patterns: `ggml-*.bin`
  - **Note**: Model files are not committed to Git due to size

### Legacy Tests

- **`BuildTest.ps1`** - PowerShell build verification script
- **`SimpleTest.ps1`** - Basic functionality test script
- **`BuildVerificationTest.md`** - Build verification documentation

## Running Tests

### Prerequisites

1. **Visual Studio 2022** with C++ development tools
2. **GGML.lib** must be built successfully
3. **Test models** downloaded to `Tests/Models/` directory

### Build and Run

```powershell
# Build all test projects
msbuild Tests\GGML\TestGGML.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild Tests\QuantizedModels\TestQuantizedModels.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild Tests\PerformanceBenchmark\PerformanceBenchmark.vcxproj /p:Configuration=Release /p:Platform=x64

# Run tests
.\Tests\GGML\x64\Release\TestGGML.exe
.\Tests\QuantizedModels\x64\Release\TestQuantizedModels.exe
.\Tests\PerformanceBenchmark\x64\Release\PerformanceBenchmark.exe
```

### Test Model Setup

Download test models to `Tests/Models/` directory:

```
Tests/Models/
├── ggml-tiny.en-q5_1.bin    # 30MB - Basic quantized model
├── ggml-tiny-q8_0.bin       # 41MB - High quality quantized
├── ggml-base-q5_1.bin       # 56MB - Medium size model
├── ggml-small.bin           # 465MB - Non-quantized reference
└── ggml-small.en-q8_0.bin   # 252MB - Large quantized model
```

## Test Results

### Expected Outputs

**TestGGML**: Basic functionality verification
- Model loading success/failure
- API responsiveness
- Memory allocation verification

**TestQuantizedModels**: Quantization compatibility
- Multiple quantization format support
- Model information extraction
- Resource management verification

**PerformanceBenchmark**: Performance analysis
- Loading time measurements
- Memory usage analysis
- Quantization efficiency metrics
- CSV report generation

### Performance Baselines

Based on Windows 11 x64, MSVC 2022, Release mode, AVX2:

| Model | Quantization | Size | Load Time | Memory |
|-------|-------------|------|-----------|---------|
| Tiny | Q5_1 | 30MB | ~83ms | ~66MB |
| Tiny | Q8_0 | 41MB | ~91ms | ~76MB |
| Base | Q5_1 | 56MB | ~101ms | ~104MB |
| Small | Q8_0 | 252MB | ~262ms | ~348MB |

## Integration with Main Project

These test projects serve as:

1. **Validation tools** for GGML integration
2. **Performance benchmarks** for optimization
3. **Regression tests** for future changes
4. **Documentation examples** for API usage

All test projects depend on `GGML.lib` and will automatically build it as needed.

## Troubleshooting

### Common Issues

**"File not found" errors**: Ensure test models are downloaded to `Tests/Models/`

**Link errors**: Verify GGML.lib builds successfully with all required symbols

**Performance variations**: Results may vary based on hardware, background processes, and system load

### Debug Information

For debugging, use Debug configuration builds which include:
- Detailed logging output
- Symbol information for debugging
- Additional validation checks

---

**Note**: This testing framework was developed during the GGML integration spike validation and serves as both verification tools and reference implementations for future development.
