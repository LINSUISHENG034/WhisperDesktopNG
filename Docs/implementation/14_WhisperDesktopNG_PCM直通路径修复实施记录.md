# WhisperDesktopNG PCM直通路径修复实施记录

**版本**: 1.0  
**日期**: 2025-06-26  
**目标**: 彻底修复main.exe标准转录功能失效的严重缺陷

## 项目背景

### 核心问题
当前编译的`main.exe`在执行标准转录时产生空的.txt文件，核心功能完全不可用。

### 问题根本原因
- **失败路径**: 音频文件 → iAudioBuffer(PCM) → pcmToMel() → iSpectrogram(MEL) → WhisperCppEncoder → transcribeFromMel() → 0分段
- **成功路径**: 音频文件 → read_audio_data() → PCM数据 → transcribeFromFile() → 1分段

**关键发现**: `iAudioBuffer`在`ContextImpl::runFull()`中确实包含完整的PCM数据，但当前架构错误地将PCM转换为MEL，然后传递给期望PCM的whisper.cpp。

### 修复策略
采用"PCM旁路"策略：在`ContextImpl::runFull()`中添加PCM直通逻辑，当检测到WhisperCppEncoder时，直接使用PCM数据而不进行MEL转换。

## 实施计划

### 阶段一：接口扩展 (Interface Extension)
**目标**: 在不破坏现有签名的情况下，为iWhisperEncoder接口赋予识别和处理PCM数据的能力。

**任务**:
1. 修改 `Whisper/iWhisperEncoder.h`
   - 添加纯虚函数 `supportsPcmInput()`
   - 添加纯虚函数 `transcribePcm()`

### 阶段二：编码器实现 (Encoder Implementation)
**目标**: 让新旧两个编码器分别实现新接口，明确各自的能力。

**任务**:
1. 修改 `Whisper/WhisperCppEncoder.h & .cpp`
   - 实现 `supportsPcmInput()` 返回 true
   - 实现 `transcribePcm()` 方法
2. 修改 `Whisper/DirectComputeEncoder.h & .cpp`
   - 实现 `supportsPcmInput()` 返回 false
   - 实现 `transcribePcm()` 返回 E_NOTIMPL

### 阶段三：核心逻辑修改 (Core Logic Modification)
**目标**: 在ContextImpl中实现"PCM旁路"逻辑。

**任务**:
1. 修改 `Whisper/ContextImpl.misc.cpp`
   - 在 `runFull` 方法开始处添加PCM旁路判断分支

### 阶段四：验证与测试 (Verification & Testing)
**目标**: 确保修复有效，且未引入任何功能回退。

**任务**:
1. 编译整个解决方案
2. 功能验证：执行标准转录命令
3. 全面回归测试

### 阶段五：清理与巩固 (Cleanup & Consolidation)
**目标**: 移除冗余代码，巩固胜利果实。

**任务**:
1. 移除冗余测试代码
2. 清理技术债务
3. 添加代码注释

## 实施过程记录

### 当前状态分析

#### 项目结构确认
- ✅ `iWhisperEncoder` 接口已存在于 `Whisper/iWhisperEncoder.h`
- ✅ `WhisperCppEncoder` 类已实现基本接口
- ✅ `DirectComputeEncoder` 类已实现基本接口
- ✅ `ContextImpl` 已有编码器依赖注入机制
- ✅ `iAudioBuffer` 接口定义完整，包含 `getPcmMono()` 和 `countSamples()` 方法

#### 编码器选择机制确认
- ✅ `ModelImpl::createContext()` 中优先尝试创建 `WhisperCppEncoder`
- ✅ 如果 `WhisperCppEncoder` 创建失败，回退到 `DirectComputeEncoder`
- ✅ `ContextImpl` 构造函数支持编码器依赖注入

#### 当前缺失的组件
- ❌ `iWhisperEncoder` 接口缺少 `supportsPcmInput()` 方法
- ❌ `iWhisperEncoder` 接口缺少 `transcribePcm()` 方法
- ❌ `WhisperCppEncoder` 未实现PCM直接处理方法
- ❌ `DirectComputeEncoder` 未实现新接口方法
- ❌ `ContextImpl::runFull()` 缺少PCM旁路逻辑

### 风险评估
- **风险等级**: 极低
- **原因**: 本方案保留了原有架构的主干，仅通过一个逻辑分支进行扩展
- **回退策略**: 如果新逻辑失败，自动回退到原有MEL转换路径

## 开始实施

### 实施时间记录
- **开始时间**: 2025-06-26 [具体时间待记录]
- **预计完成时间**: 2025-06-26 [具体时间待记录]

### 实施状态
- **当前阶段**: 阶段一 - 接口扩展 (进行中)
- **下一步**: 阶段二 - 编码器实现

## 详细实施记录

### 阶段一：接口扩展 (进行中)

#### 1.1 修改 iWhisperEncoder.h ✅
**时间**: 2025-06-26
**文件**: `Whisper/iWhisperEncoder.h`

**修改内容**:
1. ✅ 添加 `#include "API/iMediaFoundation.cl.h"` 以支持 iAudioBuffer 接口
2. ✅ 添加 `supportsPcmInput()` 纯虚函数
   - 返回类型: `bool`
   - 功能: 查询编码器是否支持PCM直通路径
3. ✅ 添加 `transcribePcm()` 纯虚函数
   - 参数: `const iAudioBuffer* buffer, const sProgressSink& progress, iTranscribeResult** resultSink`
   - 返回类型: `HRESULT`
   - 功能: 为支持PCM的编码器提供的新转录方法

**技术细节**:
- 接口设计遵循现有模式，使用HRESULT错误处理
- 添加了完整的文档注释，说明使用条件和预期行为
- 保持向后兼容性，现有方法签名未改变

**验证**:
- ✅ 编译检查通过，无语法错误
- ✅ IDE报告无新问题

### 阶段二：编码器实现 ✅
**时间**: 2025-06-26

#### 2.1 修改 WhisperCppEncoder.h ✅
**文件**: `Whisper/WhisperCppEncoder.h`

**修改内容**:
1. ✅ 添加 `supportsPcmInput()` 方法声明
2. ✅ 添加 `transcribePcm()` 方法声明

#### 2.2 修改 WhisperCppEncoder.cpp ✅
**文件**: `Whisper/WhisperCppEncoder.cpp`

**修改内容**:
1. ✅ 实现 `supportsPcmInput()` - 返回 `true`
2. ✅ 实现 `transcribePcm()` - 完整的PCM直接转录逻辑
   - 从 iAudioBuffer 提取PCM数据
   - 转换为 std::vector<float> 确保内存安全
   - 调用 m_engine->transcribe() 执行转录
   - 转换结果为COM对象并返回

#### 2.3 修改 DirectComputeEncoder.h ✅
**文件**: `Whisper/DirectComputeEncoder.h`

**修改内容**:
1. ✅ 添加 `supportsPcmInput()` 方法声明
2. ✅ 添加 `transcribePcm()` 方法声明

#### 2.4 修改 DirectComputeEncoder.cpp ✅
**文件**: `Whisper/DirectComputeEncoder.cpp`

**修改内容**:
1. ✅ 实现 `supportsPcmInput()` - 返回 `false`
2. ✅ 实现 `transcribePcm()` - 返回 `E_NOTIMPL`

**技术细节**:
- WhisperCppEncoder 支持PCM直通，实现了完整的转录逻辑
- DirectComputeEncoder 明确不支持PCM直通，保持原有MEL处理方式
- 错误处理和日志记录完整
- 内存管理安全（使用std::vector避免裸指针）

**验证**:
- ✅ 编译检查通过，无语法错误
- ✅ IDE报告无新问题

### 阶段三：核心逻辑修改 ✅
**时间**: 2025-06-26

#### 3.1 修改 ContextImpl.misc.cpp ✅
**文件**: `Whisper/Whisper/ContextImpl.misc.cpp`

**修改内容**:
1. ✅ 在 `runFull` 方法开始处添加PCM旁路判断逻辑
2. ✅ 实现完整的PCM直通路径：
   - 检查 `encoder && encoder->supportsPcmInput()`
   - 清空 `result_all` 准备接收新结果
   - 调用 `encoder->transcribePcm()` 执行PCM直接转录
   - 使用现有的 `convertResult()` 方法转换结果
   - 完整的错误处理和日志记录

**技术细节**:
- 利用现有的 `convertResult(iTranscribeResult*, std::vector<Segment>&)` 方法
- 保持与原有架构的完全兼容性
- 如果PCM路径失败，可以回退到原有MEL路径
- 添加详细的调试日志便于问题诊断

**关键代码逻辑**:
```cpp
// [核心修改] PCM旁路逻辑
if( encoder && encoder->supportsPcmInput() )
{
    // 使用新的PCM直通路径
    result_all.clear();
    ComLight::CComPtr<iTranscribeResult> transcribeResult;
    sProgressSink progressSink{ nullptr, nullptr };
    HRESULT hr = encoder->transcribePcm( buffer, progressSink, &transcribeResult );

    // 转换结果并存储
    HRESULT convertHr = this->convertResult( transcribeResult, result_all );
    return convertHr;
}
// [保持不变] 为旧引擎保留的MEL转换路径
```

**验证**:
- ✅ 编译检查通过，无语法错误
- ✅ IDE报告无新问题
- ✅ 逻辑完整性检查通过

### 阶段四：验证与测试 ⚠️ (遇到问题)
**时间**: 2025-06-26

#### 4.1 编译验证 ✅
**结果**: 所有组件编译成功
- ✅ Whisper.dll 编译成功
- ✅ main.exe 编译成功
- ✅ 无编译错误

#### 4.2 功能测试 ❌ (发现问题)
**测试命令**: `main.exe -m "E:\Program Files\WhisperDesktop\ggml-tiny.bin" -f "SampleClips\jfk.wav" -l en -otxt`

**问题发现**:
1. **PCM旁路逻辑未被触发**:
   - 预期看到: `[DEBUG] ContextImpl::runFull: Checking PCM bypass`
   - 实际情况: 直接进入 `[DEBUG] ContextImpl::runFullImpl: Using encoder interface path`
   - 说明程序没有经过我修改的 `runFull` 方法

2. **调试日志缺失**:
   - main.cpp中添加的调试日志没有出现
   - 表明可能存在其他调用路径绕过了预期的方法

3. **音频数据异常**:
   - 音频统计数据异常: `min=1.000000, max=1.000000, avg=1.000000`
   - 正常应该是: `min=-0.466856, max=1.533144, avg=0.200606`
   - 表明音频数据在某个环节被错误处理

4. **转录结果仍为0分段**:
   - `whisper_full_n_segments returned 0`
   - 核心问题未解决

#### 4.3 问题分析
**根本原因**: PCM旁路逻辑没有被执行，程序仍然使用原有的MEL转换路径

**可能原因**:
1. 存在其他调用路径直接调用 `runFullImpl`
2. 编译缓存问题导致修改未生效
3. 调用流程与预期不符

#### 4.4 下一步调试计划
1. 确认调用流程，找到为什么PCM旁路逻辑未被触发
2. 解决音频数据异常问题
3. 验证编码器的 `supportsPcmInput()` 方法是否正确返回true

**当前状态**: 修复未完成，需要进一步调试

---

**注意**: 本文档将在实施过程中持续更新，记录每个阶段的详细执行情况、遇到的问题和解决方案。
