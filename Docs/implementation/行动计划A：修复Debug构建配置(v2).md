# **行动计划A：修复Debug构建配置 (v2)**

## **1\. 当前项目面临的核心问题**

目前，WhisperDesktopNG项目正面临一个根本性的开发困境：**我们缺乏一个有效的调试环境**。具体表现为：

* **黑盒开发**：我们无法通过设置断点来单步跟踪代码，导致对WhisperCppEncoder等新模块的内部工作流程一无所知。  
* **静默失败**：程序在转录时卡死或输出空文件，但我们无法捕获到任何具体的错误信息或异常。  
* **效率低下**：任何微小的代码修改都需要重新编译整个项目才能验证，反馈周期极长，这使得开发工作异常艰难且充满挫败感。

## **2\. 整体解决方案概述**

为了走出困境，我们采纳了 **“调试优先”** 的核心战略。该战略要求我们暂停对具体应用功能的修复，转而优先投入所有精力**打造一个透明、可观测、可调试的开发环境**。只有拥有了“手术灯”和“内窥镜”，我们才能精准地定位并根除潜藏在系统深处的Bug。

## **3\. 任务A：修复Debug构建的战略意义**

**任务A是整个“调试优先”战略的基石和绝对前提**。如果说“调试优先”是我们的作战计划，那么**任务A就是我们攻坚战的第一步，也是最关键的一步**。

* **解锁一切**：没有一个可用的Debug构建环境，后续的日志记录、单元测试、集成测试都无从谈起。  
* **告别猜测**：它将使我们从“靠日志猜测”的原始开发模式，升级到“用断点洞察”的现代化高效开发模式。  
* **效率革命**：一个稳定的Debug环境将彻底改变我们的开发流程，将反馈周期从数分钟缩短到数秒，极大地提升我们解决问题的能力和信心。

## **4\. 任务A的主要目标（已更新）**

本任务采用分阶段目标，确保我们能快速建立起核心调试能力：

1. **首要目标**：**优先打通 main.exe 命令行工具的Debug编译与调试链路。** 将其作为我们隔离问题、验证核心库的“手术台”。  
2. **最终目标**：将Debug配置成功扩展至整个解决方案，实现对主程序 WhisperDesktop.exe 的单步调试。

## **5\. 任务A的关键任务分解（已更新）**

为了聚焦并快速突破，我们将任务分解调整为以 main.exe 为中心：

| 任务 ID | 关键任务 | 状态 |
| :---- | :---- | :---- |
| **A.1** | **为 main.exe 修复编译与链接** | ⬜ 未开始 |
| **A.2** | **为whisper.cpp创建Debug编译配置** | ⬜ 未开始 |
| **A.3** | **配置并启动 main.exe 调试会话** | ⬜ 未开始 |
| **A.4** | **将Debug配置扩展至完整解决方案** | ⬜ 未开始 |

### **任务A.1详解：为 main.exe 修复编译与链接**

* **目标**：解决所有导致 Examples/main 项目编译失败的问题。  
* **步骤**：  
  1. **修复头文件路径**：采用更健壮的头文件包含策略。在main项目的属性 C/C++ \-\> General \-\> Additional Include Directories 中，添加 ComLightLib 等库的根目录，而不是使用易变的相对路径 ../。  
  2. **分析运行时库（CRT）冲突**：在 main 项目的 Debug 配置中，确认其 C/C++ \-\> Code Generation \-\> Runtime Library 设置为 **Multi-threaded Debug DLL (/MDd)** 或其他Debug选项。我们必须记录下这个值。

### **任务A.2详解：为whisper.cpp创建Debug编译配置**

* **目标**：编译出一个与 main.exe 使用**完全相同运行时库**的whisper.cpp静态库（.lib）的Debug版本。  
* **步骤**：  
  1. 找到whisper.cpp的CMakeLists.txt文件。  
  2. 修改或添加CMake指令，确保在生成VS项目时，为Debug配置设置与 **任务A.1** 中记录值相同的运行时库标志（如 /MDd）。  
     \# 示例: 在CMakeLists.txt中添加类似逻辑  
     set(CMAKE\_MSVC\_RUNTIME\_LIBRARY "MultiThreadedDebugDLL") \# 对应 /MDd

  3. 重新生成解决方案，编译出whisper.cpp的Debug版本静态库。

### **任务A.3详解：配置并启动 main.exe 调试会话**

* **目标**：在Visual Studio中，将main.exe作为调试目标，并成功启动调试会话。  
* **步骤**：  
  1. 在解决方案资源管理器中，右键点击 main 项目，选择 Set as Startup Project。  
  2. 在 main.cpp 的 main 函数入口处设置一个断点。  
  3. 点击 Local Windows Debugger 启动调试。

### **任务A.4详解：将Debug配置扩展至完整解决方案**

* **目标**：将在 main.exe 上验证通过的修复方法，应用到 Whisper.dll 和 WhisperDesktop.exe 等项目中。  
* **步骤**：  
  1. 重复**任务A.1**和**A.2**的检查与修复过程，确保 Whisper.dll 和 whisper.cpp 的Debug配置兼容。  
  2. 在 Whisper.dll 的项目属性中，配置链接器，使其在Debug模式下链接到whisper.cpp的Debug静态库。  
  3. 将启动项目设置回 WhisperDesktop。

## **6\. 任务A的验收标准（已更新）**

### **中期验收标准 (Milestone Acceptance Criteria)**

**能够在 main.cpp 的 main 函数入口处设置断点，然后在Visual Studio中以Debug模式启动 main.exe，并使程序执行时能够在该断点处成功中断。**

### **最终验收标准 (Final Acceptance Criteria)**

**能够在 ContextImpl::runFullImpl 函数的第一行成功设置断点，然后在Visual Studio中以Debug模式启动 WhisperDesktop.exe，并使程序执行时能够在该断点处成功中断。**