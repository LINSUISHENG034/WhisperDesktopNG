# WhisperDesktopNG - Modern whisper.cpp Integration Project

[![Build Status](https://img.shields.io/badge/build-in%20progress-yellow)](https://github.com/LINSUISHENG034/WhisperDesktopNG)
[![Help Wanted](https://img.shields.io/badge/help-wanted-red)](https://github.com/LINSUISHENG034/WhisperDesktopNG/issues)
[![whisper.cpp](https://img.shields.io/badge/whisper.cpp-integration-blue)](https://github.com/ggml-org/whisper.cpp)

**🚨 SEEKING COMMUNITY HELP 🚨**

This project aims to integrate the latest [ggml-org/whisper.cpp](https://github.com/ggml-org/whisper.cpp) with quantized model support into a high-performance Windows DirectCompute implementation. **We have encountered a complex technical issue and are seeking experienced developers to help solve it.**

## 🎯 Project Goal

Modernize the original [Const-me/Whisper](https://github.com/Const-me/Whisper) project by:
- Integrating latest whisper.cpp with quantized model support
- Maintaining high-performance DirectCompute GPU acceleration
- Implementing streaming processing capabilities
- Providing a clean, maintainable codebase

## 🔧 Current Status

### ✅ What's Working
- **Complete project structure analysis** - Full understanding of codebase architecture
- **WhisperCppEncoder adapter framework** - Bridge between DirectCompute and whisper.cpp
- **Comprehensive debugging infrastructure** - Detailed logging and diagnostic tools
- **Build system** - Project compiles successfully with all dependencies
- **Model loading** - whisper.cpp models load correctly (ggml-base.bin, ggml-tiny.bin tested)
- **Audio preprocessing** - Audio files are correctly processed (16kHz, mono, PCM format verified)
- **whisper.cpp initialization** - Library initializes without errors

### ❌ Critical Issue: Zero Transcription Results

**The Problem**: Despite all components working correctly, `whisper_full_n_segments()` consistently returns 0, meaning no transcription segments are generated.

**What We've Verified**:
- ✅ Audio files are valid (same files work with original implementation)
- ✅ Models are correct (same models work with original implementation)  
- ✅ whisper.cpp library functions properly (no errors, proper initialization)
- ✅ Audio data reaches whisper.cpp correctly (88,000 samples, proper format)
- ✅ Language detection works (`auto-detected language: en`)
- ✅ whisper_full() returns success (0)
- ❌ **But whisper_full_n_segments() returns 0 - no transcription output**

## 🔍 Technical Details

### Architecture Overview
```
Audio Input → DirectCompute Processing → WhisperCppEncoder → whisper.cpp → Results
                                                ↑
                                        THIS IS WHERE WE NEED HELP
```

### Key Components
- **CWhisperEngine**: Core whisper.cpp wrapper with parameter management
- **WhisperCppEncoder**: Adapter implementing iWhisperEncoder interface
- **ModelImpl**: Factory pattern for encoder selection
- **DirectCompute Pipeline**: High-performance GPU audio processing

### Debugging Evidence
We have implemented comprehensive debugging and verified:

1. **Audio Data Statistics**:
   ```
   Audio stats - min=-0.123456, max=0.234567, avg=0.001234, size=88000
   ```

2. **whisper.cpp Parameters**:
   ```
   strategy=WHISPER_SAMPLING_GREEDY (0)
   n_threads=8
   language="en" 
   no_speech_thold=0.30 (lowered for better detection)
   suppress_blank=false
   ```

3. **Execution Flow**:
   ```
   whisper_full() → returns 0 (success)
   whisper_full_n_segments() → returns 0 (no segments)
   ```

## 🆘 Where We Need Help

### Primary Question
**Why does whisper_full() succeed but whisper_full_n_segments() returns 0?**

### Specific Areas for Investigation
1. **whisper.cpp Parameter Configuration** - Are we missing critical parameters?
2. **Audio Data Format** - Is there a subtle format issue whisper.cpp doesn't like?
3. **Memory Management** - Could there be memory corruption affecting results?
4. **whisper.cpp Version Compatibility** - Are we using incompatible versions?
5. **Threading Issues** - Could there be race conditions in the whisper.cpp calls?

### What We've Tried
- ✅ Multiple models (tiny, base, medium)
- ✅ Different sampling strategies (GREEDY, BEAM_SEARCH)
- ✅ Various parameter combinations (temperature, thresholds)
- ✅ Audio format verification (ffprobe confirms 16kHz mono PCM)
- ✅ Fixed whisper_full double-call bug
- ✅ Enhanced error handling and logging
- ❌ **Still getting 0 segments**

## 🚀 Quick Start for Contributors

### Prerequisites
- Visual Studio 2022 (Community Edition works)
- Windows 10/11 x64
- DirectX 11 compatible GPU

### Build Instructions
```bash
git clone https://github.com/LINSUISHENG034/WhisperDesktopNG.git
cd WhisperDesktopNG
# Open WhisperCpp.sln in Visual Studio
# Build in Debug x64 configuration
```

### Test the Issue
```bash
# Download a model to E:/Program Files/WhisperDesktop/
# Run the test
.\Examples\main\x64\Debug\main.exe -m "E:/Program Files/WhisperDesktop/ggml-base.bin" -f "SampleClips/jfk.wav" -l en -otxt
```

**Expected**: Transcription of JFK speech  
**Actual**: Empty output, countSegments=0

## 📚 Documentation

Comprehensive documentation available in `Docs/implementation/`:
- **09_行动计划I**: Complete interactive debugging methodology (29 breakpoint tests)
- **WhisperDesktopNG_项目结构分析报告**: Full project architecture analysis
- **04-07_行动计划系列**: Systematic debugging approaches and findings

## 🤝 How to Help

### For whisper.cpp Experts
- Review our parameter configuration in `Whisper/CWhisperEngine.cpp`
- Check our audio data handling in `Whisper/WhisperCppEncoder.cpp`
- Analyze our whisper_full() usage patterns

### For DirectCompute Experts  
- Review the audio pipeline in DirectCompute components
- Check memory management between DirectCompute and whisper.cpp
- Verify data format consistency

### For General C++ Developers
- Review our debugging methodology
- Suggest additional diagnostic approaches
- Help with code review and architecture improvements

## 💡 Bounty Consideration

We are open to discussing compensation for developers who can help solve this critical issue. Please reach out if you have relevant expertise and are interested in contributing.

## 📞 Contact

- **GitHub Issues**: [Create an issue](https://github.com/LINSUISHENG034/WhisperDesktopNG/issues) for technical discussions
- **Discussions**: Use GitHub Discussions for general questions and brainstorming

## 🙏 Acknowledgments

- Original [Const-me/Whisper](https://github.com/Const-me/Whisper) project for the excellent DirectCompute foundation
- [ggml-org/whisper.cpp](https://github.com/ggml-org/whisper.cpp) for the modern whisper implementation
- The open source community for potential contributions

---

**This project represents significant effort in modern whisper.cpp integration. With community help, we can create a powerful, high-performance speech recognition solution for Windows.**
