# WhisperDesktop 安全恢复计划

**创建时间**: 2025-06-29 19:30:00  
**目标**: 安全地从upstream恢复WhisperDesktop源文件，避免破坏现有架构  
**风险等级**: 中等 - 需要谨慎处理冲突

## 📋 **冲突风险分析**

### 🚨 **高风险冲突点**

1. **API接口冲突**
   - **风险**: Upstream WhisperDesktop可能使用旧版COM接口
   - **影响**: 编译错误、链接失败
   - **缓解**: 创建适配层或更新接口调用

2. **编译配置冲突**
   - **风险**: 预处理器定义、编译标志不匹配
   - **影响**: 编译错误、运行时异常
   - **缓解**: 仔细对比和调整项目配置

3. **依赖关系冲突**
   - **风险**: 链接库版本不匹配
   - **影响**: 链接错误、运行时崩溃
   - **缓解**: 更新项目引用和依赖配置

### ⚠️ **中等风险冲突点**

1. **头文件路径冲突**
   - **风险**: #include路径可能不同
   - **影响**: 编译错误
   - **缓解**: 调整包含路径配置

2. **资源文件冲突**
   - **风险**: .rc文件、图标等可能不兼容
   - **影响**: 资源编译错误
   - **缓解**: 手动合并资源文件

## 🛡️ **分阶段安全恢复策略**

### 阶段1: 环境准备和备份 (5分钟)

#### 1.1 创建安全备份
```bash
# 备份当前项目状态
git stash push -m "Backup before WhisperDesktop recovery"
git tag backup-before-desktop-recovery

# 创建专用分支进行恢复工作
git checkout -b feature/whisper-desktop-recovery
```

#### 1.2 分析upstream差异
```bash
# 获取upstream最新状态
git fetch upstream

# 分析WhisperDesktop相关文件差异
git diff HEAD upstream/master -- Examples/WhisperDesktop/
```

### 阶段2: 选择性文件恢复 (15分钟)

#### 2.1 恢复核心源文件
**优先级1 - 必需文件**:
```bash
# 恢复基础源文件
git checkout upstream/master -- Examples/WhisperDesktop/stdafx.cpp
git checkout upstream/master -- Examples/WhisperDesktop/stdafx.h
git checkout upstream/master -- Examples/WhisperDesktop/WhisperDesktop.cpp
git checkout upstream/master -- Examples/WhisperDesktop/framework.h
git checkout upstream/master -- Examples/WhisperDesktop/targetver.h
```

**优先级2 - 功能文件**:
```bash
# 恢复应用程序逻辑文件
git checkout upstream/master -- Examples/WhisperDesktop/AppState.cpp
git checkout upstream/master -- Examples/WhisperDesktop/AppState.h
git checkout upstream/master -- Examples/WhisperDesktop/LoadModelDlg.cpp
git checkout upstream/master -- Examples/WhisperDesktop/LoadModelDlg.h
git checkout upstream/master -- Examples/WhisperDesktop/TranscribeDlg.cpp
git checkout upstream/master -- Examples/WhisperDesktop/TranscribeDlg.h
```

#### 2.2 恢复UI相关文件
```bash
# 恢复对话框和UI组件
git checkout upstream/master -- Examples/WhisperDesktop/CaptureDlg.cpp
git checkout upstream/master -- Examples/WhisperDesktop/CaptureDlg.h
git checkout upstream/master -- Examples/WhisperDesktop/ModelAdvancedDlg.cpp
git checkout upstream/master -- Examples/WhisperDesktop/ModelAdvancedDlg.h
git checkout upstream/master -- Examples/WhisperDesktop/CircleIndicator.cpp
git checkout upstream/master -- Examples/WhisperDesktop/CircleIndicator.h
```

#### 2.3 恢复工具类文件
```bash
# 恢复Utils目录
git checkout upstream/master -- Examples/WhisperDesktop/Utils/
```

### 阶段3: 冲突检测和解决 (20分钟)

#### 3.1 编译测试
```bash
# 尝试编译，检测冲突
msbuild Examples\WhisperDesktop\WhisperDesktop.vcxproj /p:Configuration=Release /p:Platform=x64 /verbosity:minimal
```

#### 3.2 常见冲突解决方案

**冲突类型1: 头文件包含错误**
```cpp
// 可能需要调整的包含路径
#include "../../Whisper/API/whisperWindows.h"  // 调整为正确路径
#include "../../Whisper/API/iContext.cl.h"     // 调整为正确路径
```

**冲突类型2: COM接口版本不匹配**
```cpp
// 可能需要更新的接口调用
// 旧版本: context->runFull(params)
// 新版本: context->runFull(params, buffer)
```

**冲突类型3: 预处理器定义冲突**
```cpp
// 可能需要添加的定义
#define BUILD_BOTH_VERSIONS 0
#define BUILD_HYBRID_VERSION 0
```

### 阶段4: 适配层实现 (30分钟)

#### 4.1 创建兼容性适配器
如果发现API不兼容，创建适配层：

```cpp
// WhisperDesktop/Adapters/ApiAdapter.h
#pragma once
#include "../../Whisper/API/iContext.cl.h"

namespace WhisperDesktop {
    class ApiAdapter {
    public:
        // 适配新旧API差异
        static HRESULT RunFullCompat(Whisper::iContext* context, 
                                   const Whisper::sFullParams& params);
    };
}
```

#### 4.2 更新项目配置
```xml
<!-- 可能需要添加的包含路径 -->
<AdditionalIncludeDirectories>
  $(ProjectDir);
  $(SolutionDir)Whisper\API\;
  $(SolutionDir)GGML\;
  %(AdditionalIncludeDirectories)
</AdditionalIncludeDirectories>

<!-- 可能需要添加的库依赖 -->
<AdditionalDependencies>
  GGML.lib;
  %(AdditionalDependencies)
</AdditionalDependencies>
```

### 阶段5: 验证和测试 (15分钟)

#### 5.1 编译验证
```bash
# 完整编译测试
msbuild Examples\WhisperDesktop\WhisperDesktop.vcxproj /p:Configuration=Release /p:Platform=x64

# 检查生成的可执行文件
dir x64\Release\WhisperDesktop.exe
```

#### 5.2 基础功能测试
```bash
# 启动应用程序测试
.\x64\Release\WhisperDesktop.exe

# 检查基本UI功能
# - 应用程序启动
# - 模型加载对话框
# - 基本界面响应
```

## 🔄 **回滚策略**

### 快速回滚
如果遇到无法解决的冲突：
```bash
# 回滚到备份状态
git checkout main
git stash pop  # 恢复之前的工作状态
```

### 部分回滚
如果只有部分文件有问题：
```bash
# 回滚特定文件
git checkout HEAD -- Examples/WhisperDesktop/problematic_file.cpp
```

## 📊 **成功标准**

### 最小成功标准
- ✅ WhisperDesktop.exe 编译成功
- ✅ 应用程序能够启动
- ✅ 基本UI界面显示正常

### 完整成功标准
- ✅ 所有源文件恢复完成
- ✅ 编译无错误和警告
- ✅ 应用程序功能正常
- ✅ 能够加载模型并进行转录

## 🚨 **应急预案**

### 预案A: 创建最小化桌面应用
如果恢复失败，创建简化版本：
```cpp
// 最小化的WhisperDesktop.cpp
#include "stdafx.h"
#include "../../Whisper/API/whisperWindows.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    // 最基本的窗口和模型加载功能
    return 0;
}
```

### 预案B: 使用命令行工具替代
如果GUI应用无法恢复，使用现有的命令行工具进行QA测试：
- 使用 `Examples/main/main.exe` 进行核心功能测试
- 创建批处理脚本模拟GUI操作

## 📝 **执行检查清单**

- [ ] 创建备份和分支
- [ ] 分析upstream差异
- [ ] 恢复核心源文件
- [ ] 恢复UI相关文件
- [ ] 恢复工具类文件
- [ ] 编译测试
- [ ] 解决冲突
- [ ] 实现适配层（如需要）
- [ ] 更新项目配置
- [ ] 完整编译验证
- [ ] 基础功能测试
- [ ] 文档更新

---

**执行时间预估**: 1.5小时  
**风险等级**: 中等  
**成功概率**: 85%  
**回滚时间**: 5分钟
