---
name: Help Wanted - whisper.cpp Integration Issue
about: Help us solve the critical whisper_full_n_segments() returning 0 issue
title: '[HELP WANTED] '
labels: ['help wanted', 'bug', 'whisper.cpp']
assignees: ''
---

## 🆘 Help Wanted: whisper_full_n_segments() Returns 0

**Thank you for considering helping with this critical issue!**

### Quick Summary
- whisper_full() executes successfully (returns 0)
- whisper_full_n_segments() consistently returns 0 (no transcription segments)
- All components appear to work correctly individually
- Same audio files and models work with original implementation

### Your Expertise Area
Please check the area where you can help:
- [ ] whisper.cpp library expertise
- [ ] DirectCompute/GPU programming
- [ ] Audio processing and format handling
- [ ] C++ memory management and debugging
- [ ] Windows development and COM interfaces
- [ ] Other: _______________

### What We've Already Verified
- [ ] Audio files are valid (16kHz, mono, PCM format)
- [ ] Models load correctly (ggml-base.bin, ggml-tiny.bin tested)
- [ ] whisper.cpp initializes without errors
- [ ] Audio data reaches whisper.cpp (88,000 samples confirmed)
- [ ] Language detection works ("auto-detected language: en")
- [ ] whisper_full() returns 0 (success)
- [ ] Fixed whisper_full double-call bug
- [ ] Tried multiple parameter combinations

### Suggested Investigation Areas
Please indicate which areas you'd like to investigate:
- [ ] whisper.cpp parameter configuration review
- [ ] Audio data format and preprocessing analysis
- [ ] Memory management between DirectCompute and whisper.cpp
- [ ] whisper.cpp version compatibility check
- [ ] Threading and synchronization issues
- [ ] Alternative debugging approaches

### Your Proposed Solution/Investigation
<!-- Please describe your approach to investigating or solving this issue -->

### Additional Context
<!-- Any additional information, similar experiences, or relevant expertise -->

### Availability
- [ ] I can provide code review and suggestions
- [ ] I can actively debug and test solutions
- [ ] I'm interested in a collaborative debugging session
- [ ] I'm available for: _______________

---

**Documentation**: See `Docs/implementation/09_行动计划I：交互式断点调试验证.md` for complete debugging methodology and findings.

**Quick Test**: 
```bash
.\Examples\main\x64\Debug\main.exe -m "path/to/ggml-base.bin" -f "SampleClips/jfk.wav" -l en -otxt
```
Expected: JFK speech transcription | Actual: Empty output, countSegments=0
