# WhisperCpp PCM转录使用指南

## 快速开始

### 编译项目
```bash
# 编译Whisper.dll
msbuild Whisper\Whisper.vcxproj /p:Configuration=Debug /p:Platform=x64

# 编译测试程序
msbuild Examples\main\main.vcxproj /p:Configuration=Debug /p:Platform=x64

# 复制DLL到测试目录
Copy-Item "Whisper\x64\Debug\Whisper.dll" "Examples\main\x64\Debug\" -Force
```

### 运行PCM转录测试
```bash
cd Examples\main\x64\Debug

# 测试JFK音频 (短音频)
.\main.exe --test-pcm "E:\Program Files\WhisperDesktop\ggml-tiny.bin" "F:\Projects\WhisperDesktopNG\SampleClips\jfk.wav"

# 测试Columbia音频 (长音频)
.\main.exe --test-pcm "E:\Program Files\WhisperDesktop\ggml-tiny.bin" "F:\Projects\WhisperDesktopNG\SampleClips\columbia_converted.wav"
```

## API使用

### C++ API
```cpp
#include "CWhisperEngine.h"

// 创建配置
TranscriptionConfig config;
config.language = "en";
config.translate = false;
config.numThreads = 4;
config.enableTimestamps = true;
config.useGpu = true;
config.gpuDevice = 0;

// 创建引擎
CWhisperEngine engine("path/to/model.bin", config);

// 转录音频文件
Whisper::sProgressSink progress = {};
TranscriptionResult result = engine.transcribeFromFile("path/to/audio.wav", config, progress);

// 处理结果
if (result.success) {
    for (const auto& segment : result.segments) {
        printf("Text: %s\n", segment.text.c_str());
        printf("Time: %lld - %lld ms\n", segment.startTime, segment.endTime);
        printf("Confidence: %.3f\n", segment.confidence);
    }
}
```

### C导出接口
```cpp
// 声明
extern "C" int testPcmTranscription(const char* modelPath, const char* audioPath);

// 使用
int result = testPcmTranscription("model.bin", "audio.wav");
// 返回值: 0=成功, 1=失败
```

## 支持的音频格式

### 推荐格式
- **WAV**: 16kHz, 单声道, 16位或32位
- **采样率**: 16000 Hz (whisper.cpp标准)
- **声道**: 单声道 (自动转换)

### 自动处理
- 多声道音频自动转换为单声道
- 不同采样率自动重采样到16kHz
- 支持多种音频格式 (通过whisper.cpp的read_audio_data)

## 参数配置

### 基础参数
```cpp
TranscriptionConfig config;
config.language = "en";           // 语言代码，"auto"为自动检测
config.translate = false;         // 是否翻译为英文
config.numThreads = 4;           // CPU线程数
config.enableTimestamps = true;  // 启用时间戳
config.useGpu = true;            // 使用GPU (如果可用)
config.gpuDevice = 0;            // GPU设备ID
```

### 高级参数 (在CWhisperEngine内部设置)
```cpp
// 当前使用的激进参数设置
params.no_context = false;        // 使用上下文信息
params.suppress_blank = false;    // 不抑制空白输出
params.no_speech_thold = 0.1f;    // 低语音检测阈值
params.entropy_thold = 10.0f;     // 高熵阈值
params.logprob_thold = -10.0f;    // 低对数概率阈值
params.single_segment = true;     // 强制单分段 (可调整)
```

## 性能优化

### 模型选择
- **tiny**: 最快，准确度较低，适合实时应用
- **base**: 平衡速度和准确度
- **small**: 更高准确度，稍慢
- **medium/large**: 最高准确度，较慢

### 硬件优化
- **CPU**: 增加numThreads到CPU核心数
- **GPU**: 确保useGpu=true (需要CUDA支持)
- **内存**: 长音频需要更多内存

### 音频预处理
- 使用16kHz采样率避免重采样
- 单声道音频减少处理开销
- 去除静音段可提高准确度

## 故障排除

### 常见问题

#### 1. 模型加载失败
```
错误: whisper_init_from_file_with_params_no_state failed
解决: 检查模型文件路径和权限
```

#### 2. 音频加载失败
```
错误: read_audio_data returned false
解决: 检查音频文件格式和路径
```

#### 3. GPU不可用
```
警告: whisper_backend_init_gpu: no GPU found
解决: 检查CUDA安装或使用CPU模式
```

#### 4. 转录结果为空
```
问题: segments.size() = 0
解决: 检查音频质量，调整no_speech_thold参数
```

### 调试技巧

#### 启用详细日志
代码中已包含详细的DEBUG日志，编译Debug版本即可看到：
```
[DEBUG] CWhisperEngine::transcribeFromFile: PCM stats - min=X, max=Y, avg=Z
[DEBUG] CWhisperEngine::transcribeFromFile: whisper_full returned: 0
[DEBUG] CWhisperEngine::extractResults: whisper_full_n_segments returned N
```

#### 验证音频数据
检查PCM统计信息是否合理：
- min/max应该在[-1.0, 1.0]范围内
- avg应该接近0
- size应该等于 (音频秒数 × 16000)

#### 对比官方工具
使用官方whisper-cli.exe验证相同音频和模型：
```bash
external\whisper.cpp\build\bin\Release\whisper-cli.exe -m model.bin -f audio.wav
```

## 最佳实践

1. **测试流程**: 先用官方工具验证音频和模型
2. **参数调优**: 根据应用场景调整参数
3. **错误处理**: 检查所有返回值和异常
4. **性能监控**: 记录转录时间和内存使用
5. **质量验证**: 对比期望结果验证准确度

## 示例代码

完整的使用示例请参考：
- `Examples/main/main.cpp` - 命令行测试程序
- `Whisper/whisperCom.cpp` - testPcmTranscription函数
- `Whisper/CWhisperEngine.cpp` - transcribeFromFile方法实现
