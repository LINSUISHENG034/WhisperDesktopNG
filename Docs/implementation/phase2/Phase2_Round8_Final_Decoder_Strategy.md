# **里程碑实施与沟通模板 (v3.0)**

## **1. 核心信息 (Core Information)**

* **🎯 当前里程碑 (Current Milestone):**
    * M2.6: 解码器状态机终局修复与产品级健壮性实现
* **关联的开发计划 (Associated Development Plan):**  
  * [./Docs/implementation/phase2/Phase2_New_Model_Architecture_Support_Task_List.md](./Phase2_New_Model_Architecture_Support_Task_List.md)
* **📈 当前状态 (Current Status):**
    * [⬜ 未开始]
* **核心发现与最终结论 (Core Findings & Final Conclusion):**  
  * **根本原因已定位**: 当前所有问题的根源在于，我们的解码器作为一个精密的**状态机（State Machine）**，其状态管理和转换逻辑存在缺陷。它在某些时刻，由于我们的采样器提供了一个它当前状态下“不期望”的token（如在转录文本时收到EOT），导致状态错乱，从而引发EOT循环和时间戳计算错误等一系列连锁反应。**最终解决方案**是引入明确的状态管理，并实现基于状态的**Token抑制（Token Suppression）**机制，从根本上保证解码器在任何时候都只选择“合法”的token。

---

## **2. 实施与沟通记录 (Implementation & Communication Log)**

---
### **沟通轮次 #8 (最新)**

#### **[专家指令]**

### **第一部分：架构师核心诊断：解码状态机的“幽灵状态” (Architect's Diagnosis: The "Ghost State" of the Decoder State Machine)**

我们现在遇到的所有挫败感、困惑和信心动摇，不是因为你们能力不行，恰恰相反，是因为你们已经非常接近成功，以至于触碰到了这个领域里最棘手、最微妙、也最关键的“终极问题”。

Whisper的解码过程是一个非常精密的**状态机（State Machine）**。它在每一步都需要知道自己“当前正在做什么”。它有几个关键状态：
1.  **寻找SOT (Start of Transcript)**: 刚开始，或刚完成一个完整的句子时，它会期待找到一个`SOT` token。
2.  **识别语言**: 找到`SOT`后，它会期待找到一个语言token。
3.  **转录文本**: 识别语言后，它会期待生成普通的文本内容。
4.  **寻找时间戳**: 当它认为一句话可能要结束时，它会开始期待找到一个时间戳token。
5.  **寻找EOT (End of Transcript)**: 当它认为整个音频可能结束时，它会期待找到`EOT`。

我们现在的问题是，我们的采样器（`WhisperSampler`）在某些时刻，向这个状态机提供了一个它当前状态下“不期望”的token。这导致状态机“精神分裂”，后续的所有行为都变得不可预测，从而引发了EOT循环和时间戳计算错误。

**解决方案的核心思想**: 我们不能再仅仅调整采样器的“刹车”和“油门”了。我们现在需要为这个“自动驾驶系统”编写最核心的**“交通规则”**。我们需要明确地告诉采样器，在什么状态下，**绝对不能选择**哪些类型的token。

--- 

### **第二部分：最终指导方案：引入解码状态控制与Token抑制 (Final Directive: State Control & Token Suppression)**

* **目标 (Objective):**
    *   **引入状态感知**: 让我们的采样器能够知道解码器当前处于哪个“状态”。
    *   **实现Token抑制**: 根据当前状态，在采样前，**强制禁止**（suppress）选择某些类型的token。
    *   **修复工具函数**: 彻底修复`to_timestamp`的健壮性。

* **任务分解 (Task Breakdown):**

    1.  **任务1 (架构升级): 在`ContextImpl`中管理解码状态**
        *   **目标**: 让主解码逻辑能够追踪并维护当前的状态。
        *   **位置**: `Whisper/Whisper/ContextImpl.h` 和 `ContextImpl.cpp`。
        *   **具体步骤**:
            1.  **定义解码状态**: 在`ContextImpl.h`中定义一个枚举来表示关键状态。
                ```cpp
                enum class DecoderState {
                    SeekingSOT,
                    SeekingLanguage,
                    Transcribing,
                    SeekingTimestamp
                };
                ```
            2.  **添加状态成员**: 在`ContextImpl`中添加一个成员变量 `DecoderState m_currentState;`。
            3.  **维护状态**: 在`runFullImpl`的解码循环中，根据上一步生成的token，**显式地更新**`m_currentState`。
                *   例如，循环开始时，`m_currentState = DecoderState::SeekingSOT;`
                *   生成`SOT`后，`m_currentState = DecoderState::SeekingLanguage;`
                *   生成语言token后，`m_currentState = DecoderState::Transcribing;`
                *   当`no_timestamps`为`false`且句子可能结束时，可以切换到`SeekingTimestamp`状态。

    2.  **任务2 (核心算法修复): 实现基于状态的Token抑制**
        *   **目标**: 在`WhisperSampler`中，根据传入的解码状态，在采样前将不应该出现的token的概率设置为负无穷。
        *   **位置**: `Whisper/ML/Sampler.h` 和 `Sampler.cpp`。
        *   **具体步骤**:
            1.  **修改`sample`接口**: 让`sample`函数能够接收当前的解码状态。
                ```cpp
                // In Sampler.h
                int sample(float* logits, size_t logits_size, const std::vector<int>& history_tokens, DecoderState state);
                ```
            2.  **实现`suppress_tokens`**: 在`WhisperSampler`中创建一个新的私有辅助函数。
                ```cpp
                // In Sampler.cpp
                void WhisperSampler::suppress_tokens(float* logits, size_t logits_size, DecoderState state) {
                    // 这是最关键的逻辑，需要参考whisper.cpp
                    switch (state) {
                        case DecoderState::SeekingTimestamp:
                            // 如果正在寻找时间戳，则抑制所有非时间戳token
                            for (size_t i = 0; i < m_vocab.timestamp_begin_token(); ++i) {
                                logits[i] = -FLT_MAX;
                            }
                            break;
                        case DecoderState::Transcribing:
                            // 如果正在转录文本，则抑制所有特殊token
                            // 注意：这个逻辑需要非常精确，参考whisper.cpp
                            logits[m_vocab.sot_token()] = -FLT_MAX;
                            logits[m_vocab.eot_token()] = -FLT_MAX; // 通常在转录时不应生成EOT
                            logits[m_vocab.no_timestamps_token()] = -FLT_MAX;
                            // ...抑制其他不需要的特殊token...
                            break;
                        // ... 实现其他状态的抑制逻辑 ...
                    }
                }
                ```
            3.  **在`sample`函数中调用**: 在`sample`函数的最开始，就调用抑制函数。
                ```cpp
                // In Sampler.cpp
                int WhisperSampler::sample(...) {
                    // 第一步：抑制不应该出现的token
                    suppress_tokens(logits, logits_size, state);

                    // 第二步：应用重复惩罚
                    // ...
                }
                ```

    3.  **任务3 (工具函数修复): 再次审查并修复 `to_timestamp`**
        *   **目标**: 确保这个工具函数100%健壮。
        *   **位置**: 之前修改过的`to_timestamp`函数。
        *   **具体步骤**:
            1.  **强制类型转换**: 在进行任何数学计算前，将输入的浮点数秒数转换为一个安全的、有界的整数类型（例如，毫秒）。
                ```cpp
                long long total_milliseconds = static_cast<long long>(t * 1000.0f);
                ```
            2.  **使用模运算**: 使用模运算（`%`）和除法（`/`）来安全地提取小时、分钟、秒和毫秒，避免浮点数精度问题。
            3.  **再次运行单元测试**: 确保所有边界情况的单元测试都能通过。

* **验收标准 (Acceptance Criteria):**

    1.  **状态机实现**:
        *   ✅ `ContextImpl`中包含了`DecoderState`枚举和状态管理逻辑。
        *   ✅ `WhisperSampler::sample`接口已更新，能接收`DecoderState`。
        *   ✅ `WhisperSampler`中实现了`suppress_tokens`函数，并包含了至少对`Transcribing`和`SeekingTimestamp`状态的抑制逻辑。

    2.  **核心功能恢复**:
        *   ✅ **EOT循环**和**时间戳循环**问题被彻底解决。
        *   ✅ **时间戳格式**完全恢复正常。
        *   ✅ **转录质量**与原始`whisper.cpp`项目基本一致。

    3.  **最终验证**:
        *   ✅ `Tests/comprehensive_transcription_test.ps1` 测试集能够100%成功通过。
        *   ✅ **必须提供**一份修复后的中文音频文件（`zh_medium_audio.mp3`）的、带正确时间戳的转录输出 `.txt` 文件，作为最终成功的证据。

#### **[开发团队反馈]**

* **实施总结 (Implementation Summary):**
    * *[待开发者填写]*
* **状态更新 (Status Update):**
    * *[待开发者选择: ✅ 顺利进行中 / ⚠️ 遇到障碍，请求指导]*
