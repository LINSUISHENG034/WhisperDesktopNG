# **WhisperDesktopNG：核心功能根本修复与架构优化方案**

## **版本 1.7: 最终方案 \- 切片隔离测试 (Final Version \- Sliced Isolation Testing)**

**日期**: 2025-06-26

**状态**: 架构已成功。本方案将挑战“问题仅在于音频预处理”的核心假设，采用“切片隔离测试”的系统化方法，对整个转录流程进行模块化验证，以快速、精确地定位并修复最终障碍。

### **A. 诊断思维升级 (Upgrading the Diagnostic Mindset)**

先前的v1.6方案基于一个核心假设：我们的whisper\_full调用本身是正确的，问题仅出在输入的数据上。这是一个合理的推断，但也是一个**未经证实的、存在风险的假设**。正如团队所指出的，我们不能排除在多轮开发中，我们的whisper\_full调用环境（包括参数、上下文状态，甚至whisper.cpp库的编译与链接方式）已经与官方工具产生了差异。

**最终诊断结论：为了避免浪费时间在错误的路径上，我们必须暂停对任何单一环节的深入挖掘。转而采用“切片测试”策略，将完整的转录流程切割为三个独立的、可验证的模块，并逐一进行测试，以证据锁定问题所在的“切片”。**

### **B. 三大切片定义 (The Three Slices)**

我们将整个流程分解为以下三个关键“切片”：

1. **切片A：音频数据流 (The Audio Data Stream)**  
   * **内容**: 从文件加载到最终送入whisper\_full的std::vector\<float\> PCM数据。  
   * **核心问题**: 这份数据是否与“黄金标准”有差异？(这是v1.6的关注点)  
2. **切片B：核心引擎调用 (The Core Engine Call)**  
   * **内容**: whisper\_context对象的状态、传递给whisper\_full的whisper\_full\_params结构体，以及whisper\_full函数本身的调用。  
   * **核心问题**: 我们的调用参数、上下文状态是否与官方工具完全一致？  
3. **切片C：编译与环境 (The Build & Environment)**  
   * **内容**: whisper.cpp库被编译成.lib并链接到Whisper.dll的方式，以及main.exe加载和执行Whisper.dll时的进程环境。  
   * **核心问题**: 是否存在ABI不兼容、内存损坏、或依赖冲突等环境问题？

### **C. 最终行动计划：逐个击破**

本计划的核心是快速验证每个切片的健康度。

#### **阶段一：验证切片B和C的健康度 \- “黄金数据回放”测试 (Highest Priority)**

**目标**: 使用完全标准的“黄金数据”来测试我们的核心引擎调用和编译环境，从而一举判断问题是否出在音频预处理之外。

1. **提取“黄金标准”PCM样本 (同v1.6)**:  
   * 修改官方whisper-cli.exe的源码，在调用whisper\_full之前，将pcm32f向量的内容完整写入二进制文件 golden\_standard.pcm。  
2. **创建“黄金数据回放”测试函数 (关键步骤)**:  
   * **任务**: 在你们的Whisper.dll中，创建一个新的、可导出的C函数，或者一个CWhisperEngine的公共方法。  
     // 建议在 CWhisperEngine.cpp 中添加一个公共方法  
     // 或者一个独立的C导出函数 testWithGoldenPcm(const char\* modelPath)  
     TranscriptionResult CWhisperEngine::transcribeWithGoldenPcm(const std::string& goldenPcmPath)  
     {  
         // 1\. 从磁盘读取 "golden\_standard.pcm" 文件  
         std::ifstream golden\_file(goldenPcmPath, std::ios::binary);  
         golden\_file.seekg(0, std::ios::end);  
         size\_t size\_in\_bytes \= golden\_file.tellg();  
         golden\_file.seekg(0, std::ios::beg);

         std::vector\<float\> golden\_audio(size\_in\_bytes / sizeof(float));  
         golden\_file.read(reinterpret\_cast\<char\*\>(golden\_audio.data()), size\_in\_bytes);

         // 2\. 使用这份“黄金数据”调用你现有的、内部的transcribe方法  
         // 确保这里使用的参数与之前测试时完全一致  
         logMessage("\[Golden Playback\] Transcribing with golden standard PCM data...");  
         return this-\>transcribe(golden\_audio);   
     }

   * 在main.exe中添加一个新的命令行参数（如--playback-test）来调用这个新函数。  
3. **执行并分析结果**:  
   * 运行main.exe \--playback-test。  
   * **如果测试成功（生成了正确的转录文本）**:  
     * **结论**: 恭喜！这证明了你们的**切片B（核心引擎调用）和切片C（编译与环境）是完全健康、没有问题的**。问题100%被隔离在\*\*切片A（音频数据流）\*\*中。此时，可以放心地回到v1.6中的“数据法证比对”方案，去分析你们自己生成的PCM与黄金标准之间的差异。  
   * **如果测试失败（依然返回0个分段）**:  
     * **结论**: 这是一个重大发现！它证明了**问题不在于音频数据**，而在于更深层次的**切片B或切片C**。这让你们避免了在音频预处理上浪费时间，可以直接进入下一阶段的调查。

#### **阶段二：深入调查切片B \- “参数与上下文”审计**

**目标**: (仅在阶段一失败时执行) 精确对比调用whisper\_full时的所有输入参数和上下文状态。

1. **参数全量审计**: 在你们的CWhisperEngine::transcribe方法和修改后的官方whisper-cli.exe中，在调用whisper\_full的前一刻，逐行打印出whisper\_full\_params结构体中的**每一个字段**。进行文本对比，确保没有任何一个参数存在差异。  
2. **上下文创建审计**: 对比两个项目中创建whisper\_context时所用的whisper\_context\_params是否完全一致。

#### **阶段三：终极隔离切片C \- “洁净室”测试**

**目标**: (仅在阶段一、二均失败时执行) 创建一个最简单的环境来验证Whisper.dll本身。

1. **创建全新测试项目**: 在你们的解决方案中，创建一个全新的、最小的C++控制台应用程序项目（例如MinimalTester.exe）。  
2. **最小化依赖**: 这个新项目只链接Whisper.dll，不包含任何Const-me项目的其他代码。  
3. **执行核心调用**: 在MinimalTester.exe的main函数中，加载Whisper.dll，创建CWhisperEngine实例，然后调用“黄金数据回放”测试函数。  
4. **分析**: 如果这个“洁净室”测试成功，而main.exe中的测试失败，则证明问题在于main.exe的复杂环境（如内存冲突、依赖库问题等）。如果“洁净室”测试也失败，则问题极有可能出在Whisper.dll的编译或链接配置上。

### **D. 结论**

**v1.7 是一个更科学、更严谨的调试路线图。** 它放弃了基于假设的单一路径，而是通过系统性的“切片隔离测试”，为团队提供了一个无论问题出在何处都能快速定位的框架。这个方案将确保团队的宝贵时间被用在最有价值的调查上，从而高效、精准地解决这最后一个难题。