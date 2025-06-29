# **里程碑实施与沟通模板 (v3.0)**

## **1. 核心信息 (Core Information)**

* **🎯 当前里程碑 (Current Milestone):**
    * M2.3: 采样器架构重构与健壮性最终解决方案
* **关联的开发计划 (Associated Development Plan):**  
  * [./Docs/implementation/phase2/Phase2_New_Model_Architecture_Support_Task_List.md](./Phase2_New_Model_Architecture_Support_Task_List.md)
* **📈 当前状态 (Current Status):**
    * [⬜ 未开始]
* **核心发现与最终结论 (Core Findings & Final Conclusion):**  
  * **最终解决方案已确定**: 为了从根本上解决各类token循环问题，并为未来提供稳定、可扩展的架构，我们将引入独立的`WhisperSampler`类，封装所有采样逻辑，并建立一套高质量的默认采样参数体系。

---

## **2. 实施与沟通记录 (Implementation & Communication Log)**

---
### **沟通轮次 #5 (最新)**

#### **[专家指令]**

### **第一部分：架构师的信心与决策理由 (Architect's Confidence & Rationale)**

在开始之前，我想首先对团队说：**我完全相信你们有能力成功实施这个方案。** 你们在过去几轮的迭代中，展现了精准的诊断能力、高效的执行力和严谨的测试流程，这些是完成任何复杂工程任务的核心要素。本次方案并非创造一个全新的、未知的算法，而是对现有逻辑进行一次优雅的“工程重构”，这完全在你们当前的能力范围之内。

我们之所以选择这条路，原因如下：
1.  **根本性解决问题**: “打补丁”式的修复（例如用更多的`if-else`来区分模式）只会让代码越来越复杂，未来还会出现我们意想不到的“第三层”、“第四层”问题。引入`WhisperSampler`，是将所有与采样相关的复杂性隔离到一个独立的“黑盒”中，一劳永逸。
2.  **拥抱GPU原生**: 我们不能为了复用`whisper.cpp`的CPU代码而牺牲掉我们最核心的GPU性能优势。在GPU上用HLSL或C++复刻其算法思想，才是正确的道路。
3.  **简化未来开发**: 当我们将采样逻辑封装好之后，未来无论是支持`top_k`采样，还是为桌面应用提供“高质量”、“高性能”两种模式，都只需要在`WhisperSampler`内部进行调整，而无需触碰`ContextImpl`等核心业务流程，极大地降低了维护成本。

--- 

### **第二部分：最终指导方案：实现 `WhisperSampler` (Final Directive: Implementing `WhisperSampler`)**

* **目标 (Objective):**
    *   **根本性修复**: 创建一个独立的、可配置的`WhisperSampler`类，封装所有采样逻辑，彻底解决token循环问题。
    *   **用户友好**: 为桌面应用提供一套无需用户配置的、高质量的默认采样参数。
    *   **面向未来**: 架构上支持未来扩展更复杂的采样策略，而无需修改核心解码循环。

* **任务分解 (Task Breakdown):**

    1.  **任务1: 设计并创建 `SamplingParams` 结构体**
        *   **目标**: 将所有与采样相关的参数聚合到一个结构体中，便于管理和传递。
        *   **位置**: 在 `Whisper/API/` 目录下创建一个新的头文件 `sParams.h` (如果已有类似的，可以放进去)。
        *   **实现代码 (Code)**:
            ```cpp
            // In Whisper/API/sParams.h
            #pragma once

            struct SamplingParams {
                float temperature = 0.8f;
                float repetition_penalty = 1.1f;
                int   history_size = 10; // 用于重复惩罚的历史token数量
                // 未来可以扩展 top_k, top_p 等

                // 为桌面应用等上层调用者提供一套高质量的、无需思考的默认值
                static SamplingParams defaultParams() {
                    return SamplingParams{};
                }
            };
            ```

    2.  **任务2: 实现 `WhisperSampler` 类**
        *   **目标**: 封装所有采样逻辑，包括重复惩罚、温度采样等。
        *   **位置**: 创建新的 `Whisper/ML/Sampler.h` 和 `Whisper/ML/Sampler.cpp` 文件。
        *   **实现代码 (Code)**:
            
            **`Sampler.h` (头文件)**
            ```cpp
            #pragma once
            #include <vector>
            #include "../API/sParams.h"
            #include "../API/iVocab.h"

            class WhisperSampler {
            public:
                WhisperSampler(const SamplingParams& params, const iVocab& vocab);

                // 核心采样函数
                int sample(float* logits, size_t logits_size, const std::vector<int>& history_tokens);

            private:
                SamplingParams m_params;
                const iVocab& m_vocab; // 引用词汇表以访问特殊token ID

                // 私有辅助函数
                void apply_repetition_penalty(float* logits, size_t logits_size, const std::vector<int>& history_tokens);
                void apply_temperature(float* logits, size_t logits_size);
            };
            ```

            **`Sampler.cpp` (实现文件)**
            ```cpp
            #include "stdafx.h"
            #include "Sampler.h"
            #include <algorithm> // For std::max

            WhisperSampler::WhisperSampler(const SamplingParams& params, const iVocab& vocab)
                : m_params(params), m_vocab(vocab) {}

            void WhisperSampler::apply_repetition_penalty(float* logits, size_t logits_size, const std::vector<int>& history_tokens) {
                if (m_params.repetition_penalty == 1.0f || history_tokens.empty()) {
                    return;
                }

                // 从历史记录中取最近的N个token
                size_t start_index = (history_tokens.size() > m_params.history_size) ? (history_tokens.size() - m_params.history_size) : 0;

                for (size_t i = start_index; i < history_tokens.size(); ++i) {
                    int token_id = history_tokens[i];
                    // 关键：不对特殊token（如时间戳、EOT等）进行惩罚
                    if (token_id >= m_vocab.timestamp_begin_token()) continue;

                    if (logits[token_id] > 0) {
                        logits[token_id] /= m_params.repetition_penalty;
                    } else {
                        logits[token_id] *= m_params.repetition_penalty;
                    }
                }
            }

            void WhisperSampler::apply_temperature(float* logits, size_t logits_size) {
                if (m_params.temperature == 0.0f) return; // T=0意味着贪婪采样，无需调整

                for (size_t i = 0; i < logits_size; ++i) {
                    logits[i] /= m_params.temperature;
                }
            }

            int WhisperSampler::sample(float* logits, size_t logits_size, const std::vector<int>& history_tokens) {
                // 1. 应用重复惩罚
                apply_repetition_penalty(logits, logits_size, history_tokens);

                // 2. 应用温度
                apply_temperature(logits, logits_size);

                // 3. 查找最优Token (当前仍为贪婪采样，未来可扩展)
                int best_token_id = 0;
                float max_prob = -FLT_MAX;
                for (size_t i = 0; i < logits_size; ++i) {
                    if (logits[i] > max_prob) {
                        max_prob = logits[i];
                        best_token_id = (int)i;
                    }
                }
                return best_token_id;
            }
            ```

    3.  **任务3: 在 `ContextImpl` 中集成 `WhisperSampler`**
        *   **目标**: 在主解码逻辑中使用新的采样器，彻底替换掉旧的`sampleBest`。
        *   **位置**: `Whisper/Whisper/ContextImpl.h` 和 `ContextImpl.cpp`。
        *   **实现代码 (Code)**:
            
            **`ContextImpl.h`**
            ```cpp
            // ... other includes ...
            #include "../ML/Sampler.h" // 引入新的头文件

            class ContextImpl : public ComLight::ObjectRoot<iContext> {
                // ... other members ...
                std::unique_ptr<WhisperSampler> m_sampler;
                // ...
            };
            ```

            **`ContextImpl.cpp`**
            ```cpp
            // 在构造函数或模型加载函数中初始化
            ContextImpl::ContextImpl(CComPtr<iModel> model, ...)
            {
                // ... other initializations ...
                m_sampler = std::make_unique<WhisperSampler>(SamplingParams::defaultParams(), *model->vocab());
            }

            // 在解码循环中 (例如 runFullImpl 内部)
            // ...
            // HRESULT hr = m_decoders.back()->run( ... &logits_tensor );
            // const float* logits = logits_tensor->cpuData();
            // const size_t n_logits = logits_tensor->shape.last();
            
            // 用下面这行替换掉旧的 sampleBest 调用
            int best_token_id = m_sampler->sample(const_cast<float*>(logits), n_logits, m_recent_tokens);
            // ...
            ```

* **验收标准 (Acceptance Criteria):**

    1.  **架构重构完成**:
        *   ✅ 代码库中存在新的 `WhisperSampler` 类和 `SamplingParams` 结构体。
        *   ✅ `ContextImpl` 中已使用 `WhisperSampler` 替代了旧的采样逻辑。
        *   ✅ 旧的 `sampleBest` 函数已被移除或标记为废弃。

    2.  **功能正确性**:
        *   ✅ 运行 `Tests/comprehensive_transcription_test.ps1` 中的所有测试，**所有测试用例都能生成正确的、无循环的文本输出**。
        *   ✅ **Timestamp模式** 和 **No-timestamp模式** 均能正常工作。

    3.  **简洁性与可扩展性**:
        *   ✅ 对于 `WhisperDesktop.exe` 等上层应用，无需暴露任何新的采样参数即可正常工作（因为我们使用了 `defaultParams`）。
        *   ✅ 代码结构清晰，未来如果需要支持 `top_k` 采样，只需在 `WhisperSampler` 内部修改，而无需改动 `ContextImpl` 的主流程。

#### **[开发团队反馈]**

* **实施总结 (Implementation Summary):**
    * ✅ **架构实现成功**: WhisperSampler类按专家建议完整实现，包括SamplingParams结构体和所有接口
    * ✅ **编译通过**: 所有代码正确编译，项目构建成功
    * ✅ **集成完成**: 成功集成到ContextImpl中，替换原有采样逻辑
    * ❌ **核心问题未解决**: 仍然陷入文本token循环，连续220次生成相同token `' my'` (token_id=452)

* **验收验证 (Acceptance Verification):**
    * ❌ **No-timestamp模式**: 仍然失败，陷入`' my'`文本token循环
    * ❌ **重复惩罚效果**: 概率从0.656461降至0.468297，但力度不足以阻止循环
    * ❌ **回归测试**: 无法通过，问题未解决
    * ✅ **架构质量**: 代码结构清晰，易于扩展和维护

* **关键发现 (Key Findings):**
    * **参数配置问题**: 专家建议的repetition_penalty=1.1力度太弱，无法有效阻止极端循环
    * **需要更强惩罚**: 测试显示需要1.5-2.0的惩罚系数才能打破循环
    * **温度采样限制**: temperature=0.8对于这种极端情况效果有限
    * **循环检测缺失**: 缺乏连续重复检测和自动参数调整机制

* **技术收获 (Technical Learnings):**
    * **架构设计**: 专家的封装设计确实优雅且易于维护
    * **参数调优**: 默认参数需要针对实际问题场景进行调整
    * **问题复杂性**: token循环问题比预期更复杂，需要更精细的解决方案

* **状态更新 (Status Update):**
    * ⚠️ **遇到障碍，请求指导**: 架构实现成功，但默认参数配置无法解决核心问题
    * 🔧 **需要专家指导**: 建议提供更强的采样策略或参数优化方案
    * 📊 **数据已收集**: 详细的循环行为数据可用于进一步分析