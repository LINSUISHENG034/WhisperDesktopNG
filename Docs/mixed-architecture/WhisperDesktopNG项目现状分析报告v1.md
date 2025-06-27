# WhisperDesktopNG项目问题描述

## 项目背景

我们正在开发一个基于whisper.cpp的语音转录项目WhisperDesktopNG，该项目fork自Const-me/Whisper，目标是集成最新的ggerganov/whisper.cpp以支持量化模型。

## 核心问题

**主要症状**：使用whisper.cpp进行音频转录时，`whisper_full_n_segments()`始终返回0，导致无法获得任何转录文本，但`whisper_full()`本身返回0（表示成功）。

## 技术环境

- **操作系统**：Windows
- **编译器**：Visual Studio 2022 (MSVC)
- **whisper.cpp版本**：最新的ggerganov/whisper.cpp（通过Git submodule集成）
- **模型**：ggml-tiny.bin（已验证在官方whisper.cpp中工作正常）
- **测试音频**：jfk.wav（已验证在官方whisper.cpp中可正常转录）

## 问题隔离实验结果

我们进行了两个受控实验来隔离问题根源：

### 实验J.1：参数对齐验证

- **目的**：验证参数设置是否是问题根源
- **方法**：完全复制whisper.cpp官方main.cpp的参数创建逻辑
- **结果**：使用官方"黄金标准"参数仍然得到n_segments=0

### 实验J.2：数据管道验证

- **目的**：验证音频数据处理是否是问题根源
- **方法**：绕过Const-me的音频处理管道，直接使用whisper.cpp官方的`read_audio_data()`函数
- **结果**：**成功！** n_segments=1，正确转录出文本

## 关键技术发现

| 音频加载方式 | 音频数据大小 | whisper_full结果 | n_segments | 转录结果 |
|-------------|-------------|-----------------|------------|----------|
| Const-me管道 | 88,000 samples | 0 (成功) | 0 | 失败 |
| 官方read_audio_data | 176,000 samples | 0 (成功) | 1 | 成功 |

## 具体技术细节

### 成功的配置（实验J.2）

```cpp
// 使用官方whisper.cpp的read_audio_data加载音频
std::vector<float> pcmf32;
std::vector<std::vector<float>> pcmf32s;
if (!read_audio_data(audioPath, pcmf32, pcmf32s, false)) return 1;

// 使用官方默认参数
struct whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
// no_context=true, suppress_blank=true, no_speech_thold=0.60

// 直接调用whisper.cpp C API
struct whisper_context* ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
int result = whisper_full(ctx, wparams, pcmf32.data(), (int)pcmf32.size());
int n_segments = whisper_full_n_segments(ctx); // 返回1，成功！

// 成功转录结果
const char* text = whisper_full_get_segment_text(ctx, 0);
// "And so my fellow Americans ask not what your country can do for you, ask what you can do for your country."
```

### 失败的配置（原项目）

- 使用Const-me项目的音频处理管道加载音频
- 音频数据大小只有官方方式的一半（88k vs 176k samples）
- 相同的whisper.cpp API调用，但n_segments=0

## 推测的问题原因

1. **音频重采样问题**：Const-me的音频处理可能进行了不当的重采样
2. **音频格式转换问题**：可能在PCM格式转换过程中丢失了关键信息
3. **音频预处理问题**：可能应用了某种滤波或归一化，破坏了whisper.cpp所需的音频特征
4. **采样率问题**：whisper.cpp要求16kHz采样率，Const-me管道可能处理不当

## 需要解决的问题

### 1. 根本原因分析

为什么相同的音频文件，不同的加载方式会导致数据大小差异？这种差异为什么会导致whisper.cpp无法识别语音片段？

### 2. 兼容性解决方案

如何在保持与Const-me项目兼容的前提下，修复音频处理管道？是否需要完全替换音频加载逻辑？

### 3. 技术实现建议

- 是否应该直接集成whisper.cpp的官方音频处理代码？
- 如何处理现有的COM接口和音频处理架构？
- 是否有更优雅的解决方案避免大规模重构？

### 4. 音频处理最佳实践

对于whisper.cpp，音频预处理有哪些关键要求？哪些操作可能会破坏语音识别效果？

## 项目约束

- 需要保持与现有Const-me架构的兼容性
- 需要支持Windows平台的COM接口
- 希望避免大规模重构，寻求最小化修改的解决方案

## 相关文档

- 详细实验记录：`Docs/implementation/行动计划J执行记录.md`
- 项目技术分析：`Docs/technical/` 目录下的相关文档

## 联系信息

如需更多技术细节或代码示例，请参考项目仓库中的完整实现和测试代码。

---

**文档创建时间**：2025-06-25  
**问题状态**：已通过受控实验确定根本原因，寻求最佳解决方案
