# **里程碑实施与沟通模板 (v3.0)**

## **1. 核心信息 (Core Information)**

* **🎯 当前里程碑 (Current Milestone):**
    * M2.5: 解码器状态机与特殊Token处理终局修复
* **关联的开发计划 (Associated Development Plan):**  
  * [./Docs/implementation/phase2/Phase2_New_Model_Architecture_Support_Task_List.md](./Phase2_New_Model_Architecture_Support_Task_List.md)
* **📈 当前状态 (Current Status):**
    * [⬜ 未开始]
* **核心发现与最终结论 (Core Findings & Final Conclusion):**  
  * **根本原因已定位**: 当前的转录质量问题（EOT循环、时间戳格式错误）源于两个独立的系统性缺陷：1) **算法层**：`WhisperSampler`对除时间戳外的其他特殊控制token（如EOT）缺少正确的处理逻辑。2) **工具层**：`to_timestamp`格式化函数存在健壮性bug。**最终解决方案**是双管齐下，在升级采样器算法的同时，修复工具函数，从而体系化地解决问题。

---

## **2. 实施与沟通记录 (Implementation & Communication Log)**

---
### **沟通轮次 #7 (最新)**

#### **[专家指令]**

### **第一部分：核心原理与问题解答 (Core Principles & Q&A)**

在开始修复之前，我们必须先统一对几个关键问题的认知。这能确保我们不仅“知其然”，更“知其所以然”。

**1. 任务1是否有现成的算法可以借鉴？**

**答案是：有，而且我们必须借鉴。** 我们需要的“识别所有特殊token”的算法，在`whisper.cpp`的`whisper.h`头文件中已经有了一个完美的、可以直接“翻译”的参考实现：`whisper_is_special()`。其核心思想是：**所有“特殊用途”的控制token，其ID值都被安排在词汇表的末尾。** 因此，我们只需要一个分界点（通常是`sot_token`的ID），所有ID大于或等于此值的token，都应被视为特殊token，不参与重复惩罚等文本处理。

**2. 原项目`Const-me/Whisper`是如何实现的？**

`Const-me/Whisper`项目遵循了与`whisper.cpp`完全相同的**核心思想**。它并没有重新发明轮子，而是在其C++代码和HLSL着色器中，完全基于这个“特殊token在末尾”的约定进行设计。我们之前遇到的问题，正是因为我们的`WhisperSampler`在进行“重复惩罚”时，没有完整地遵循这个约定。

**3. 为什么连背景音乐都可以识别出来？**

**答案是：这几乎完全是模型本身的能力，而不是我们代码中任何特定算法的结果。**
*   **强大的声学模型**: Whisper模型在训练时使用了海量的、包含各种噪声的真实世界音频数据，使其学会了如何从复杂的声学环境中，将人类的语音信号“分离”出来。它不是在“识别”背景音乐，而是在**“忽略”背景音乐**。
*   **注意力机制**: 模型内部的“注意力机制”像一个智能的“聚光灯”，会自动聚焦到最可能包含语音信息的音频片段上。
*   **我们的代码扮演的角色**: 我们的代码是一个**“高效的执行引擎”**。模型负责“听懂”内容（**What**），我们的代码负责“快速准确地计算和转录”（**How**）。

--- 

### **第二部分：最终指导方案：升级采样器并修复时间戳工具 (Final Directive: Upgrade Sampler & Fix Timestamp Util)**

这个方案将分为两个独立但都必须完成的任务，以求从根本上解决问题。

* **目标 (Objective):**
    *   **彻底解决Token循环**: 升级`WhisperSampler`，使其能够正确处理所有特殊控制token，根除`EOT`及其他潜在的特殊token循环。
    *   **修复时间戳格式**: 修复`to_timestamp`工具函数，确保其能将秒数正确格式化为`HH:MM:SS.mmm`格式。
    *   **恢复转录质量**: 确保转录结果恢复到或超过原始项目的水平。

* **任务分解 (Task Breakdown):**

    1.  **任务1 (核心算法修复): 升级`WhisperSampler`以识别所有特殊Token**
        *   **目标**: 让重复惩罚和Top-K采样等策略，能够正确地忽略所有不应被惩罚或限制的特殊token。
        *   **位置**: `Whisper/ML/Sampler.cpp` 和 `Whisper/API/iVocab.h`。
        *   **具体步骤**:
            1.  **在 `iVocab.h` 中扩展接口**: 我们需要一个更通用的方法来判断一个token是否是“特殊”的。
                ```cpp
                // In iVocab.h, inside the __interface iVocab
                // Add this new method
                virtual bool isSpecial( int token_id ) const = 0;
                ```
            2.  **在 `VocabImpl.h/cpp` 中实现**: 在`VocabImpl`中实现这个新接口，精确复刻`whisper.cpp`的`whisper_is_special()`逻辑。
                ```cpp
                // In VocabImpl.cpp
                #include "whisper.h" // 确保可以访问到原始C API

                bool VocabImpl::isSpecial( int token_id ) const override {
                    // 直接调用或复刻 whisper.cpp 的判断逻辑
                    // 最简单、最可靠的方式是直接使用sot_token作为分界点
                    return token_id >= m_vocab.sot_token();
                }
                ```
            3.  **在 `WhisperSampler` 中使用新接口**: 修改采样器的所有相关逻辑，用`isSpecial()`替换掉旧的、不完整的判断。
                ```cpp
                // In Sampler.cpp, apply_repetition_penalty
                // ...
                if (m_vocab.isSpecial(token_id)) continue;
                // ...
                ```

    2.  **任务2 (工具函数修复): 调试并修复 `to_timestamp` 函数**
        *   **目标**: 确保时间戳格式化函数能够正确处理各种输入，并返回正确的字符串格式。
        *   **位置**: 查找项目中名为 `to_timestamp` 或功能类似的函数（可能在某个`Utils`或`StringUtils`文件中）。
        *   **具体步骤**:
            1.  **添加输入验证**: 在函数开头，增加对输入浮点数 `t` 的检查。
                ```cpp
                #include <cmath> // For std::isfinite

                std::string to_timestamp(float t) {
                    // 健壮性检查
                    if (t < 0.0f || !std::isfinite(t)) {
                        // 如果输入是负数或无效浮点数（如NaN, infinity），返回一个明确的错误或默认值
                        return "00:00:00.000";
                    }
                    // ... 原有的格式化逻辑 ...
                }
                ```
            2.  **审查数学计算**: 仔细检查从`t`（秒）计算出小时、分钟、秒和毫秒的数学逻辑，确保没有整数溢出或类型转换问题。
            3.  **创建单元测试**: 为`to_timestamp`函数编写一个小型单元测试，覆盖以下情况：
                *   `t = 0.0f` -> `"00:00:00.000"`
                *   `t = 123.456f` -> `"00:02:03.456"`
                *   `t = 3700.0f` -> `"01:01:40.000"`
                *   `t = -1.0f` -> `"00:00:00.000"`
                *   `t = FLT_MAX` -> （根据你的错误处理逻辑，也应返回一个有效或错误提示格式）

* **验收标准 (Acceptance Criteria):**

    1.  **EOT循环彻底解决**:
        *   ✅ 运行之前失败的所有测试用例，**不再出现EOT token循环**。
        *   ✅ `WhisperSampler` 的代码已使用新的 `iVocab::isSpecial()` 接口来排除所有特殊token的重复惩罚。

    2.  **时间戳格式恢复正常**:
        *   ✅ 在Timestamp模式下，生成的转录文件中，**所有时间戳都恢复为 `HH:MM:SS.mmm` 的正确格式**。
        *   ✅ `to_timestamp` 函数已增加了健壮性检查和单元测试。

    3.  **转录质量恢复**:
        *   ✅ 使用`ggml-small.bin`模型和中文音频文件测试，生成的转录文本内容与**原始`whisper.cpp`项目的结果基本一致**。
        *   ✅ **必须提供**一份修复后的中文音频文件的转录输出 `.txt` 文件，与原始项目的输出进行对比，作为质量恢复的证据。

#### **[开发团队反馈]**

* **实施总结 (Implementation Summary):**
    * *[待开发者填写]*
* **状态更新 (Status Update):**
    * *[待开发者选择: ✅ 顺利进行中 / ⚠️ 遇到障碍，请求指导]*
