# iEncoder Interface Analysis - WhisperDesktopNG

## 概述

经过深入分析 WhisperDesktopNG 项目的代码库，发现该项目**并没有独立的 `iEncoder` 接口**。编码功能是通过 `WhisperContext` 类的 `encode` 方法实现的，该方法是转录流程中的核心组件。

## 关键发现

### 1. 编码器接口的实际实现

**实际的编码接口位置**：
- **主要实现**：`Whisper/Whisper/WhisperContext.h` 和 `WhisperContext.cpp`
- **接口方法**：`Tensor encode(Whisper::iSpectrogram& spectrogram, const sEncodeParams& encParams)`
- **调用层**：`Whisper/Whisper/ContextImpl.cpp` 中的 `HRESULT encode(iSpectrogram& mel, int seek)` 方法

### 2. 编码方法签名分析

#### WhisperContext::encode 方法
```cpp
Tensor encode(Whisper::iSpectrogram& spectrogram, const sEncodeParams& encParams);
```

**输入参数**：
1. **`Whisper::iSpectrogram& spectrogram`**：
   - 音频频谱图接口，提供 MEL 频谱数据
   - 关键方法：`makeBuffer(size_t offset, size_t length, const float** buffer, size_t& stride)`
   - 数据格式：80个MEL频带的浮点数数组

2. **`const sEncodeParams& encParams`**：
   - 编码参数结构体，包含模型配置信息
   - 定义在 `Whisper/Whisper/sEncodeParams.h`

#### ContextImpl::encode 方法
```cpp
HRESULT encode(iSpectrogram& mel, int seek);
```

**输入参数**：
1. **`iSpectrogram& mel`**：MEL频谱图数据接口
2. **`int seek`**：音频偏移量（以帧为单位）

**返回值**：`HRESULT` - 标准Windows COM错误码

### 3. 编码参数结构 (sEncodeParams)

```cpp
struct sEncodeParams
{
    uint32_t n_ctx;          // 上下文长度
    uint32_t n_mels;         // MEL频带数量（通常为80）
    uint32_t mel_offset;     // MEL数据偏移量
    uint32_t layersCount;    // 编码器层数
    uint32_t n_state;        // 状态维度
    uint32_t n_head;         // 注意力头数
    uint32_t n_audio_ctx;    // 音频上下文长度
    uint32_t n_text_state;   // 文本状态维度
    uint32_t n_text_layer;   // 文本层数
    uint32_t n_text_ctx;     // 文本上下文长度
};
```

### 4. iSpectrogram 接口规格

```cpp
__interface iSpectrogram
{
    // 创建指定偏移和长度的缓冲区，包含 length * N_MEL 个浮点数
    HRESULT makeBuffer(size_t offset, size_t length, const float** buffer, size_t& stride);
    
    // 获取频谱图长度（单位：160个输入样本 = 10毫秒音频）
    size_t getLength() const;
    
    // 如果源数据是立体声，复制指定切片到提供的向量中
    HRESULT copyStereoPcm(size_t offset, size_t length, std::vector<StereoSample>& buffer) const;
};
```

**关键常量**：
- `N_MEL = 80`：MEL频带数量
- 时间单位：160个样本 = 10毫秒音频

### 5. 编码流程分析

#### 在转录主循环中的调用序列：
1. **音频预处理**：PCM → MEL频谱图转换
2. **编码调用**：`CHECK(encode(mel, seek))`（第559行）
3. **解码循环**：基于编码结果进行文本解码
4. **结果处理**：生成分段和令牌

#### 编码内部流程：
1. **参数设置**：从模型参数构建 `sEncodeParams`
2. **GPU上传**：`melInput.create(spectrogram, encParams)`
3. **卷积处理**：`convolutionAndGelu(melInput, encParams.n_ctx)`
4. **层处理**：遍历所有编码器层
5. **后处理**：归一化和交叉注意力缓冲区预计算

### 6. 输出格式

**编码输出**：
- **类型**：`DirectCompute::Tensor`
- **用途**：为解码器提供编码特征
- **存储**：GPU内存中的张量对象

**最终转录结果**：
- **接口**：`iTranscribeResult`
- **分段数据**：`sSegment` 结构数组
- **令牌数据**：`sToken` 结构数组

## 关键设计特点

### 1. 流式处理支持
- 支持三种处理模式：`runFull`、`runStreamed`、`runCapture`
- 统一的 `runFullImpl` 内部实现
- 自适应的MEL流处理器（单线程/多线程）

### 2. GPU加速
- 基于DirectCompute的GPU计算
- 张量操作在GPU内存中进行
- 支持性能分析和调试追踪

### 3. 模块化设计
- 编码和解码分离
- 接口抽象良好
- 支持不同的频谱图实现

## 适配器设计建议

基于分析结果，为集成新的 whisper.cpp 版本，建议创建以下适配器：

### 1. 编码器适配器接口
```cpp
class WhisperCppEncoderAdapter {
public:
    HRESULT encode(iSpectrogram& spectrogram, int seek);
    // 适配现有的 ContextImpl::encode 签名
};
```

### 2. 关键适配点
1. **MEL数据转换**：iSpectrogram → whisper.cpp格式
2. **参数映射**：sEncodeParams → whisper.cpp参数
3. **结果转换**：whisper.cpp输出 → DirectCompute::Tensor
4. **错误处理**：统一的HRESULT错误码

### 3. 集成策略
- 保持现有的 `ContextImpl::encode` 接口不变
- 在内部切换到新的whisper.cpp实现
- 确保与现有流式处理和GPU管道兼容

## 结论

WhisperDesktopNG项目没有独立的 `iEncoder` 接口，而是通过 `WhisperContext::encode` 方法实现编码功能。该方法接收 `iSpectrogram` 接口提供的MEL频谱数据和编码参数，返回GPU张量格式的编码特征。理解这个架构对于成功集成新版本的whisper.cpp至关重要。
