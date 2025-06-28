# 阶段0任务清单 - 端到端量化Spike实现

## 📋 总体目标
**通过最小化端到端PoC验证核心技术假设：成功加载GGUF量化张量并在GPU上解量化**

### 🎯 关键原则
这是防止重复错误假设的最后防线！必须通过具体的端到端实现验证技术可行性。

### 📅 时间安排
**总计: 3-4天**

### 🎯 Spike目标
"加载单个量化张量并在GPU上成功解量化"

---

## Day 1: 环境准备与GGUF解析

### 任务1: 测试环境搭建 ⏱️ 2小时

#### 1.1 准备测试用GGUF文件
- [ ] 下载一个小型量化模型（如ggml-tiny-q4_0.gguf）
- [ ] 验证文件完整性和格式正确性
- [ ] 确认文件包含Q4_0量化张量

#### 1.2 创建Spike项目结构
- [ ] 在项目中创建`Spike/QuantizationSpike/`目录
- [ ] 设置独立的测试项目配置
- [ ] 配置项目依赖关系

#### 1.3 配置构建环境
- [ ] 确保可以编译现有项目
- [ ] 验证DirectX/D3D11开发环境
- [ ] 测试基础GPU功能

### 任务2: GGUF格式解析实现 ⏱️ 4小时

#### 2.1 研究GGUF文件格式规范
- [ ] 分析文件头结构（magic number, version等）
- [ ] 理解元数据存储方式（key-value pairs）
- [ ] 确定张量数据布局和偏移计算

#### 2.2 实现最小化GGUF解析器
**⚠️ 优化建议**: 只解析必要的键值对，避免过度工程

```cpp
class MinimalGGUFParser {
public:
    HRESULT parseHeader(const std::string& filePath, GGUFHeader& header);
    HRESULT findFirstQ4_0Tensor(const std::string& filePath, TensorInfo& tensor);
    HRESULT loadTensorData(const std::string& filePath, const TensorInfo& tensor, std::vector<uint8_t>& data);

private:
    // 只解析必要的键值对
    static const char* ESSENTIAL_KEYS[];  // "*.shape", "*.ggml_type"
    bool isEssentialKey(const std::string& key);
    HRESULT parseEssentialMetadata(const std::string& filePath);
};
```

**实施重点**:
- 只解析张量名称、维度(*.shape)和量化类型(*.ggml_type)
- 忽略所有其他元数据，节省解析时间
- 专注于找到第一个Q4_0张量

#### 2.3 验证解析正确性
- [ ] 成功读取文件头magic number (GGUF)
- [ ] 正确解析版本信息和元数据数量
- [ ] 找到第一个Q4_0量化张量
- [ ] 验证张量维度和数据类型

### 任务3: CPU参考实现准备 ⏱️ 2小时

#### 3.1 集成whisper.cpp CPU解量化
**⚠️ 关键建议**: 创建独立的参考检查器项目避免链接器冲突

- [ ] 创建独立的ReferenceChecker.exe项目（CMake或.vcxproj）
- [ ] 配置为Release模式编译，避免_ITERATOR_DEBUG_LEVEL不匹配
- [ ] 链接whisper.cpp库到独立项目（而非主Spike项目）
- [ ] 找到Q4_0解量化函数（dequantize_row_q4_0）
- [ ] 实现命令行接口：ReferenceChecker.exe <gguf_file> <tensor_name>
- [ ] 验证独立程序的CPU解量化功能正常

**技术要点**:
- 避免主项目与whisper.cpp的运行时库冲突
- 通过进程间通信或文件交换数据
- 保持主Spike项目的简洁性

#### 3.2 实现参考检查器
```cpp
class ReferenceChecker {
public:
    std::vector<float> getCPUDequantization(const QuantizedTensor& tensor);
    bool compareResults(const std::vector<float>& gpu, const std::vector<float>& cpu, float epsilon = 1e-6f);
private:
    void logDifferences(const std::vector<float>& gpu, const std::vector<float>& cpu);
};
```

---

## Day 2: GPU缓冲区与HLSL着色器

### 任务4: GPU缓冲区管理 ⏱️ 3小时

#### 4.1 设计量化数据GPU布局
**💡 技术指导**: 使用StructuredBuffer<uint>和位运算处理Q4_0格式

- [ ] 分析Q4_0数据格式：block_q4_0结构（32个4-bit值 + 1个FP16 scale）
- [ ] 设计输入缓冲区：StructuredBuffer<uint>，每个uint包含8个4-bit值
- [ ] 设计输出缓冲区：RWStructuredBuffer<float>，存储解量化结果
- [ ] 规划位运算提取：使用(data >> (i*4)) & 0xF提取4-bit值
- [ ] 处理scale值：从buffer的特定位置读取FP16 scale

**HLSL缓冲区设计**:
```hlsl
StructuredBuffer<uint> g_quantizedData;    // 输入：量化数据
RWStructuredBuffer<float> g_outputData;    // 输出：FP32结果
StructuredBuffer<half> g_scaleData;        // scale值（可选独立缓冲区）
```

#### 4.2 实现GPU缓冲区创建
**⚠️ 关键配置**: 正确设置D3D11_BUFFER_DESC的BindFlags和MiscFlags

```cpp
class QuantizedBufferManager {
public:
    HRESULT createInputBuffer(const QuantizedTensor& tensor, ID3D11Buffer** buffer);
    HRESULT createOutputBuffer(size_t floatCount, ID3D11Buffer** buffer);
    HRESULT uploadQuantizedData(const QuantizedTensor& tensor, ID3D11Buffer* buffer);
    HRESULT downloadResults(ID3D11Buffer* buffer, std::vector<float>& results);

private:
    HRESULT createStructuredBuffer(size_t elementSize, size_t elementCount,
                                 const void* initialData, ID3D11Buffer** buffer) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = elementSize * elementCount;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = elementSize;
        // ... 创建缓冲区
    }
};
```

#### 4.3 验证缓冲区操作
- [ ] 成功创建GPU缓冲区（输入和输出）
- [ ] 正确上传量化数据到GPU
- [ ] 验证数据完整性（上传后下载对比）

### 任务5: HLSL解量化着色器 ⏱️ 5小时

#### 5.1 研究Q4_0量化算法
- [ ] 理解Q4_0编码格式（block_q4_0结构）
- [ ] 分析解量化数学公式：f = scale * (q - 8)
- [ ] 研究whisper.cpp中的dequantize_row_q4_0实现

#### 5.2 编写最小化HLSL着色器
**🐛 调试技巧**: 使用输出缓冲区写入中间值进行"printf调试"

```hlsl
// DequantizeQ4_0_Spike.hlsl
[numthreads(256, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    // 最小化Q4_0解量化实现
    // 专注于正确性，暂不优化性能

    // 读取量化数据和scale
    uint quantData = g_quantizedData[id.x / 8];  // 8个4-bit值per uint
    half scale = g_scaleData[id.x / 32];         // 每32个值一个scale

    // 提取4-bit值
    uint shift = (id.x % 8) * 4;
    uint q = (quantData >> shift) & 0xF;

    // 解量化公式：f = scale * (q - 8)
    float result = scale * (q - 8);

    // 调试技巧：可以输出中间值而非最终结果
    // g_outputData[id.x] = q;        // 调试：输出原始q值
    // g_outputData[id.x] = scale;    // 调试：输出scale值
    g_outputData[id.x] = result;      // 正常：输出最终结果
}
```

**实施重点**:
- 专注正确性，不考虑性能优化
- 使用调试输出验证中间计算步骤
- 确保位运算和数学公式正确

#### 5.3 集成到现有着色器系统
- [ ] 添加新着色器到ComputeShaders项目
- [ ] 在eComputeShader枚举中添加DequantizeQ4_0_Spike条目
- [ ] 实现着色器编译、加载和绑定逻辑

---

## Day 3: GPU调度与端到端集成

### 任务6: GPU计算调度 ⏱️ 3小时

#### 6.1 实现着色器调度逻辑
```cpp
class QuantizationDispatcher {
public:
    HRESULT dispatchDequantizeShader(ID3D11Buffer* input, ID3D11Buffer* output, uint32_t blockCount);
    HRESULT downloadResults(ID3D11Buffer* output, std::vector<float>& results);
private:
    HRESULT bindBuffers(ID3D11Buffer* input, ID3D11Buffer* output);
    HRESULT calculateDispatchSize(uint32_t blockCount, uint32_t& groupsX);
};
```

#### 6.2 集成到现有GPU管理系统
- [ ] 使用现有Device和Context（复用Whisper/D3D/device.h）
- [ ] 复用现有的GPU资源管理机制
- [ ] 确保与现有着色器系统兼容

#### 6.3 验证GPU执行
- [ ] 着色器成功调度（无D3D错误）
- [ ] GPU调试层无警告或错误
- [ ] 正确下载结果数据到CPU

### 任务7: 端到端Spike集成 ⏱️ 3小时

#### 7.1 实现完整Spike类
```cpp
class QuantizationSpike {
public:
    HRESULT initialize();
    HRESULT proveQuantizationConcept(const std::string& ggufFile);
    void cleanup();

private:
    MinimalGGUFParser m_parser;
    ReferenceChecker m_checker;
    QuantizedBufferManager m_bufferMgr;
    QuantizationDispatcher m_dispatcher;
    
    HRESULT verifyAgainstCPUReference(const QuantizedTensor& tensor, ID3D11Buffer* gpuResult);
};
```

#### 7.2 端到端流程测试
- [ ] 完整流程无崩溃运行
- [ ] 所有中间步骤返回成功状态
- [ ] 错误处理机制有效（文件不存在、格式错误等）

---

## Day 4: 验证与优化

### 任务8: 结果验证与调试 ⏱️ 4小时

#### 8.1 CPU vs GPU结果对比
**🔍 强化验证**: 检查NaN/Inf并提供首次失败的详细上下文

- [ ] 实现robust的浮点比较（epsilon = 1e-6f）
- [ ] 添加NaN和Inf检查（isnan(), isinf()）
- [ ] 实现首次失败报告：记录失败索引和前后5-10个值
- [ ] 分析差异原因（位运算错误、数学公式错误等）
- [ ] 调试GPU着色器实现（使用GPU调试工具和中间值输出）

**增强的比较函数**:
```cpp
bool compareResults(const std::vector<float>& gpu, const std::vector<float>& cpu, float epsilon = 1e-6f) {
    for (size_t i = 0; i < gpu.size(); ++i) {
        // 检查NaN和Inf
        if (std::isnan(gpu[i]) || std::isinf(gpu[i])) {
            logFirstFailure(i, gpu, cpu, "GPU produced NaN/Inf");
            return false;
        }

        // epsilon比较
        if (std::abs(gpu[i] - cpu[i]) > epsilon) {
            logFirstFailure(i, gpu, cpu, "Value mismatch");
            return false;
        }
    }
    return true;
}

void logFirstFailure(size_t index, const std::vector<float>& gpu, const std::vector<float>& cpu, const char* reason) {
    // 记录失败索引和周围5-10个值的上下文
    size_t start = (index >= 5) ? index - 5 : 0;
    size_t end = std::min(index + 5, gpu.size());
    // ... 详细日志输出
}
```

#### 8.2 正确性验证
- [ ] GPU结果与CPU参考在1e-6 epsilon内一致
- [ ] 验证多个不同张量（不同大小、不同位置）
- [ ] 确保结果的可重复性（多次运行一致）

#### 8.3 错误情况处理
- [ ] 测试文件不存在情况
- [ ] 测试格式错误情况（非GGUF文件）
- [ ] 测试GPU资源不足情况
- [ ] 测试无Q4_0张量的文件

### 任务9: 性能基准与文档 ⏱️ 2小时

#### 9.1 基础性能测量
- [ ] 测量端到端延迟（从文件读取到结果验证）
- [ ] 分析各阶段耗时（解析、上传、GPU计算、下载、验证）
- [ ] 与预期性能对比（目标：单张量 < 10ms）

#### 9.2 Spike结果文档
- [ ] 记录成功/失败状态和具体原因
- [ ] 文档化发现的技术问题和解决方案
- [ ] 为后续阶段提供具体建议和风险评估

---

## 🎯 成功标准验收

### 必须达成 (Go/No-Go决策点)
- [ ] ✅ **GGUF解析成功**: 能够正确解析文件头和找到Q4_0张量
- [ ] ✅ **GPU着色器执行**: HLSL着色器成功运行无错误
- [ ] ✅ **结果正确性**: GPU结果与CPU参考在1e-6 epsilon内一致
- [ ] ✅ **流程稳定性**: 端到端流程可重复执行无崩溃

### 期望达成
- [ ] 🎯 **性能合理**: 单张量解量化延迟 < 10ms
- [ ] 🎯 **错误处理**: 优雅处理各种错误情况
- [ ] 🎯 **代码质量**: 代码结构清晰，便于扩展到完整实现

### 风险应对策略
- **如果Spike完全失败**: 立即停止，重新评估技术方案，考虑替代架构
- **如果部分成功**: 分析失败原因，调整后续计划，可能需要延长时间
- **如果完全成功**: 继续执行阶段1计划，技术风险大幅降低

---

## 📁 交付物清单

### 1. 代码实现
- `Spike/QuantizationSpike/` 完整项目目录
- 可编译运行的测试程序
- 相关HLSL着色器文件

### 2. 测试结果
- Spike执行日志和输出
- 性能测量数据（各阶段耗时）
- 正确性验证报告（CPU vs GPU对比）

### 3. 技术文档
- Spike实施总结报告
- 发现的技术问题和解决方案
- 后续阶段的具体建议和风险评估

---

**文档创建时间**: 2025年6月27日
**预计执行时间**: 3-4天
**关键里程碑**: 端到端量化张量处理验证成功
