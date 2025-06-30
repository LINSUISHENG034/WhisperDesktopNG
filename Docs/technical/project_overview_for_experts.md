# WhisperDesktopNG 项目概览 - 专家参考

## 🎯 项目简介

**WhisperDesktopNG** 是基于 Const-me/Whisper 的增强版本，目标是集成 ggerganov/whisper.cpp 以支持量化模型，同时保持原有的 Windows GPU 性能优势。

### 核心目标
- 支持 GGML 量化模型（更小的模型文件，更快的加载）
- 保持原有的 DirectCompute GPU 加速性能
- 实现混合引擎架构，根据模型类型自动选择最优引擎
- 提供高质量的中文语音转录和分段功能

## 🏗️ 系统架构

### 整体架构图
```
WhisperDesktopNG
├── WhisperDesktop.exe          # GUI 应用程序
├── Whisper.dll                 # 核心转录引擎
│   ├── ContextImpl             # 转录上下文管理
│   ├── ModelImpl               # 模型加载和管理
│   ├── Sampler                 # Token 采样策略
│   └── HybridLoader            # 混合模型加载器
├── GGML.lib                    # whisper.cpp 静态库
└── ComLightLib.lib             # COM 接口支持
```

### 双引擎架构
1. **原有引擎**：Const-me/Whisper (DirectCompute GPU 加速)
2. **新增引擎**：ggerganov/whisper.cpp (GGML 量化模型支持)
3. **智能选择**：根据模型格式自动选择最优引擎

## 🔄 程序运行流程

### 1. 初始化阶段
```cpp
// 模型检测和加载
bool isGGMLModel = detectGGMLFormat(modelPath);
if (isGGMLModel) {
    // 使用 whisper.cpp 引擎
    context = whisper_init_from_file(modelPath);
} else {
    // 使用原有 DirectCompute 引擎
    context = loadDirectComputeModel(modelPath);
}
```

### 2. 音频处理流程
```
音频文件 → PCM 解码 → MEL 频谱图 → 编码器 → 特征向量
```
- **输入**：音频文件（MP3, WAV, M4A 等）
- **预处理**：16kHz 采样率，80维 MEL 特征
- **编码**：生成 1500 维特征向量（30秒音频）

### 3. 解码和分段流程
```cpp
// 核心解码循环
while (!finished) {
    // 1. 获取下一个 token 的概率分布
    logits = decoder.forward(encodedFeatures, previousTokens);
    
    // 2. 采样策略决定下一个 token
    token = sampler.sample(logits);
    
    // 3. 检查是否为时间戳 token
    if (isTimestampToken(token)) {
        // 创建新的分段
        createSegment(currentText, lastTimestamp, token.timestamp);
        lastTimestamp = token.timestamp;
        currentText.clear();
    } else {
        // 累积文本内容
        currentText += token.text;
    }
}
```

## 🎯 核心技术挑战

### 问题描述
我们的中文语音转录功能正常，但分段效果远不如原项目：

**原项目效果**（14个自然分段）：
```
[00:00:00.000 --> 00:00:02.800] 不用开火10分钟就能吃上下 饭
[00:00:02.800 --> 00:00:04.840] 菜 茄子 辣椒
[00:00:04.840 --> 00:00:06.880] 细红柿 180度 10分钟
...
```

**我们的实现**（2个长分段）：
```
[00:00:00.080 --> 00:00:12.040] 不用开火十分钟就能吃上下飯菜节子啦叫...
[00:00:12.040 --> 00:00:25.300] 所啦叫给他思开西方式去皮用讲到直接见...
```

### 根本原因
**时间戳 token 生成不足**：我们只在开头和结尾生成时间戳 token，缺少中间的时间戳 token 来触发自然分段。

## 🔍 技术分析

### Token 生成模式对比

**原项目 token 序列**（推测）：
```
[_TT_0] 不用开火10分钟就能吃上下 [_TT_140] 饭 [_TT_168] 菜 茄子 辣椒 [_TT_242] ...
```

**我们的 token 序列**（实际）：
```
[_TT_5] 不用开火十分钟就能吃上下飯菜节子啦...（98个文本token）...[_TT_1416]
```

### 关键差异
1. **时间戳频率**：原项目每2-8个文本token就有一个时间戳token
2. **分段触发**：原项目基于自然时间戳token分段，我们缺少中间时间戳
3. **采样策略**：原项目使用简单有效的策略，我们可能过于复杂

## 🔧 技术栈信息

### 开发环境
- **平台**：Windows 11, Visual Studio 2022
- **编程语言**：C++20, DirectCompute HLSL
- **GPU支持**：DirectX 11, NVIDIA CUDA 兼容
- **音频处理**：Media Foundation, 16kHz 采样率

### 依赖库
- **whisper.cpp**：GGML 量化模型支持
- **DirectCompute**：GPU 加速计算
- **Media Foundation**：音频解码和处理
- **COM Light**：轻量级 COM 接口

### 测试环境
- **测试模型**：ggml-small.bin (244MB, 中文优化)
- **测试音频**：30秒中文语音样本
- **参考结果**：原项目的14个自然分段输出

## 📊 当前状态

### ✅ 已完成功能
1. **GGML 模型集成**：成功加载和使用量化模型
2. **混合引擎架构**：自动选择最优引擎
3. **中文转录功能**：转录准确率正常
4. **基础分段功能**：能产生基本的时间戳分段
5. **性能保持**：GPU 加速性能未受影响

### 🚨 存在问题
1. **分段粒度过粗**：只能产生2个长分段
2. **时间戳分布不均**：缺少中间时间戳
3. **语义分段缺失**：无法在语音停顿点自然分段

### 🔧 已尝试的解决方案
| 方案 | 方法 | 结果 | 问题 |
|------|------|------|------|
| 强制时间戳 | 每N个token强制生成 | 13个分段 | 时间戳断层 |
| 调整采样参数 | 降低动态采样阈值 | 无效果 | 条件从未满足 |
| wrapSegment | 启用后处理分段 | 100个字符级分段 | 非语义分段 |

## 🎯 专家支持需求

### 核心技术问题
1. **时间戳生成机制**：原项目如何决定在何时生成时间戳token？
2. **语音停顿检测**：是否存在专门的算法识别语音停顿？
3. **采样策略优化**：如何设计采样器促进自然时间戳生成？

### 期望的专家指导
1. **技术分析**：对我们当前实现的详细分析
2. **改进方案**：具体的技术改进路径和算法建议
3. **参数调优**：最优的采样参数设置
4. **代码审查**：关键模块的代码审查和优化建议

## 📁 项目资源

### 代码仓库
- **主仓库**：WhisperDesktopNG
- **关键文件**：
  - `Whisper/Whisper/ContextImpl.cpp` (主要分段逻辑)
  - `Whisper/ML/Sampler.cpp` (采样器实现)
  - `Examples/main/main.cpp` (测试程序)

### 测试数据
- **音频文件**：`Tests/Audio/zh_short_audio.mp3`
- **参考结果**：`Tests/Audio/ggml-small/zh_short_audio_debugconsole.txt`
- **测试脚本**：`Tests/comprehensive_transcription_test.ps1`

### 技术文档
- **详细报告**：`Docs/technical/expert_consultation_request.md`
- **项目概览**：`Docs/technical/project_overview_for_experts.md` (本文档)
- **调试记录**：`Docs/implementation/phase2/stage4_segmentation_optimization.md`

---

**文档目的**：为专家团队提供 WhisperDesktopNG 项目的完整技术背景，便于快速理解项目架构、核心挑战和技术需求。

**联系方式**：WhisperDesktopNG 开发团队
**文档版本**：v1.0
**创建日期**：2025-06-30
