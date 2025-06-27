# WhisperDesktopNG

A high-performance Windows implementation of OpenAI's Whisper automatic speech recognition (ASR) model with **quantized model support**.

This project extends the original [Const-me/Whisper](https://github.com/Const-me/Whisper) DirectCompute implementation by integrating [whisper.cpp](https://github.com/ggerganov/whisper.cpp) to support modern quantized models.

## 🎉 **NEW: GGML Quantized Model Support**

**WhisperDesktopNG** now includes full support for GGML quantized models through successful integration of whisper.cpp 1.7.6:

### Key Features
- **✅ Quantized Model Support**: Q4_0, Q5_1, Q8_0 and other quantization formats
- **✅ Significant Performance Gains**: 27-46% reduction in model size and memory usage
- **✅ Fast Loading Times**: 83ms for tiny models, 262ms for small quantized models
- **✅ Multiple Model Sizes**: Support for tiny, base, small, and medium models
- **✅ Backward Compatibility**: Original DirectCompute implementation remains unchanged

### Performance Benchmarks

| Model | Quantization | Size | Load Time | Memory Usage |
|-------|-------------|------|-----------|--------------|
| Tiny | Q5_1 | 30MB | 83ms | 66MB |
| Tiny | Q8_0 | 41MB | 91ms | 76MB |
| Base | Q5_1 | 56MB | 101ms | 104MB |
| Small | Q8_0 | 252MB | 262ms | 348MB |

*Tested on Windows 11 x64 with MSVC 2022, Release mode, AVX2 enabled*

### Technical Achievement

This integration successfully resolved **35 unresolved external symbols** through systematic problem-solving:

1. **Expert-guided diagnosis**: Identified x86 architecture-specific code requirements
2. **Mixed C/C++ compilation strategy**: Precise control over compilation methods for different files
3. **Comprehensive testing**: 5-iteration performance benchmarks with statistical validation
4. **Complete documentation**: Full technical reports for future reference

**For developers facing similar GGML integration challenges**, see our detailed technical documentation in `Docs/implementation/` for proven solutions and best practices.

## Quick Start

### Download and Run
1. Download WhisperDesktop.zip from the "Releases" section
2. Extract the ZIP file
3. Run WhisperDesktop.exe
4. Download a model when prompted (recommend `ggml-medium.bin` for best results)

### Model Selection
- **Tiny models** (30-41MB): Fast processing, suitable for real-time applications
- **Base models** (56MB): Balanced performance and accuracy
- **Small models** (252-465MB): Higher accuracy for offline processing

## Core Features

### Performance
- **Vendor-agnostic GPGPU** based on DirectCompute (Direct3D 11 compute shaders)
- **Mixed F16/F32 precision** for optimal performance on modern GPUs
- **Much faster than OpenAI's implementation** - up to 2.3x speedup on tested hardware
- **Low memory usage** with quantized models

### Audio Support
- **Media Foundation integration** supports most audio and video formats
- **Audio capture** from most Windows-compatible devices
- **Voice activity detection** for automatic speech detection
- **Real-time processing** capabilities

### Developer-Friendly
- **COM-style API** for easy integration
- **C# wrapper available** on NuGet as WhisperNet
- **PowerShell support** for scripting scenarios
- **Built-in performance profiler** for optimization

## System Requirements

- **Platform**: 64-bit Windows 8.1 or newer (tested on Windows 10/11)
- **GPU**: Direct3D 11.0 capable GPU (any hardware GPU from 2012 onwards)
- **CPU**: AVX1 and F16C support required
- **Memory**: Varies by model size (66MB for tiny, up to 561MB for large models)

## Build Instructions

### Prerequisites
- Visual Studio 2022 (Community Edition or higher)
- Windows 10/11 SDK

### Build Steps
1. Clone this repository
2. Open `WhisperCpp.sln` in Visual Studio 2022
3. Switch to `Release` configuration
4. Build and run `CompressShaders` project (in Tools folder)
5. Build `Whisper` project for the native DLL
6. Build test projects in `Tests/` folder for validation

### GGML Integration Notes

The quantized model support is implemented as a separate static library (`GGML.lib`):

**Critical build settings**:
```xml
<PreprocessorDefinitions>WIN32_LEAN_AND_MEAN;NOMINMAX;_CRT_SECURE_NO_WARNINGS;GGML_USE_CPU</PreprocessorDefinitions>
<EnableEnhancedInstructionSet>AdvancedVectorExtensions2</EnableEnhancedInstructionSet>
<AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
```

For detailed technical documentation, see `Docs/implementation/Phase0_03_GGML_Integration_Complete_Report.md`.

## Testing

The project includes comprehensive test suites in the `Tests/` directory:

- **`Tests/GGML/`** - Basic GGML functionality tests
- **`Tests/QuantizedModels/`** - Quantized model compatibility tests  
- **`Tests/PerformanceBenchmark/`** - Performance analysis and benchmarking

Run tests to verify your build:
```powershell
.\Tests\GGML\x64\Release\TestGGML.exe
.\Tests\QuantizedModels\x64\Release\TestQuantizedModels.exe
.\Tests\PerformanceBenchmark\x64\Release\PerformanceBenchmark.exe
```

## Documentation

- **Technical Reports**: `Docs/implementation/` - Complete integration documentation
- **Performance Analysis**: `Docs/technical/` - Detailed performance studies
- **Test Documentation**: `Tests/README.md` - Testing framework guide

## Contributing

This project demonstrates successful integration of modern quantized models with high-performance Windows implementations. The technical approach and solutions may be valuable for other projects facing similar GGML integration challenges.

Key contributions include:
- Systematic approach to resolving complex linking issues
- Mixed C/C++ compilation strategies for GGML integration
- Comprehensive performance benchmarking methodology
- Complete documentation of technical challenges and solutions

## License

This project builds upon the original Const-me/Whisper implementation and incorporates whisper.cpp. Please refer to the respective licenses of the base projects.

## Acknowledgments

- **Const-me** for the original high-performance Windows Whisper implementation
- **Georgi Gerganov** for whisper.cpp and the GGML framework
- **OpenAI** for the original Whisper model
- **Technical experts** who provided guidance during the integration process

---

**Project Status**: ✅ Active development with successful quantized model integration

For technical questions or integration challenges, refer to the comprehensive documentation in the `Docs/` directory.
