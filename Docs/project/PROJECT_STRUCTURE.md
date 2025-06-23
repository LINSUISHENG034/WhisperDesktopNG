# WhisperDesktopNG 项目结构说明

## 项目根目录结构

```
WhisperDesktopNG/
├── ComLightLib/                 # COM 轻量级库
├── ComputeShaders/              # 计算着色器
├── Deploy/                      # 部署相关文件
├── Docs/                        # 项目文档
│   ├── documentation_standards/ # 文档标准
│   ├── implementation/          # 实施总结文档
│   ├── project/                 # 项目相关文档
│   └── technical/               # 技术分析文档
├── Examples/                    # 示例项目
│   ├── MicrophoneCS/           # C# 麦克风示例
│   ├── TranscribeCS/           # C# 转录示例
│   ├── WhisperDesktop/         # 桌面应用示例
│   └── main/                   # 主要示例
├── SampleClips/                # 示例音频文件
├── Scripts/                    # 构建和部署脚本（统一管理）
│   ├── Build/                  # 构建相关脚本
│   ├── Test/                   # 测试脚本
│   ├── Deploy/                 # 部署脚本
│   ├── README.md               # 脚本说明文档
│   └── USAGE.md                # 使用指南
├── Tests/                      # 测试相关文件
├── Tools/                      # 开发工具
├── Whisper/                    # 核心 Whisper 库
├── WhisperNet/                 # .NET 绑定
├── WhisperPS/                  # PowerShell 模块
├── external/                   # 外部依赖（Git 子模块）
│   └── whisper.cpp/           # 最新版本的 whisper.cpp
├── include/                    # 生成的头文件
│   └── whisper_cpp/           # whisper.cpp 头文件
├── lib/                        # 生成的静态库
│   └── x64/Release/           # 64位 Release 版本库文件
└── WhisperCpp.sln             # Visual Studio 解决方案文件
```

## 构建系统结构

### Scripts/ 目录（统一脚本管理）

```
Scripts/
├── README.md                   # 详细说明文档
├── USAGE.md                    # 快速使用指南
├── PROJECT_STRUCTURE.md        # 本文件
├── Build/                      # 构建相关脚本
│   ├── build_whisper.bat      # 主要构建脚本
│   ├── build_whisper_static.bat # 备用构建脚本
│   ├── clean_build.bat        # 清理脚本
│   └── whisper_build/         # 构建工作目录
│       ├── CMakeLists.txt     # CMake 配置文件
│       └── build/             # 构建输出（自动生成）
├── Test/                       # 测试脚本
│   ├── BuildTest.ps1          # 构建验证测试
│   └── SimpleTest.ps1         # 简单功能测试
└── Deploy/                     # 部署脚本
    └── copy-binaries.cmd      # 二进制文件复制脚本
```

### 生成文件结构

```
lib/x64/Release/               # 静态库文件
├── whisper.lib               # 主要语音识别库
├── ggml.lib                  # 机器学习基础库
├── ggml-base.lib             # GGML 基础组件
└── ggml-cpu.lib              # CPU 优化后端

include/whisper_cpp/           # 头文件
├── whisper.h                 # 主要头文件
├── ggml.h                    # GGML 头文件
└── [其他相关头文件]
```

## 关键特性

### 1. 统一脚本管理
- 所有构建、测试、部署脚本集中在 `Scripts/` 目录
- 清晰的功能分类和目录结构
- 详细的文档说明和使用指南

### 2. 现代化构建系统
- 基于 CMake 的 whisper.cpp 构建
- 支持最新版本的 ggml-org/whisper.cpp
- 自动化的库文件和头文件管理

### 3. Git 子模块集成
- `external/whisper.cpp` 作为 Git 子模块
- 便于跟踪和更新到最新版本
- 保持与上游项目的同步

### 4. 多语言支持
- C++ 核心库 (Whisper/)
- .NET 绑定 (WhisperNet/)
- PowerShell 模块 (WhisperPS/)
- C# 示例项目 (Examples/)

## 使用指南

### 快速开始
```bash
# 初始化子模块
git submodule update --init --recursive

# 构建 whisper.cpp 静态库
Scripts\Build\build_whisper.bat

# 构建整个解决方案
msbuild WhisperCpp.sln /p:Configuration=Release /p:Platform=x64
```

### 清理构建
```bash
# 清理所有构建文件
Scripts\Build\clean_build.bat
```

### 运行测试
```bash
# 构建验证测试
powershell Scripts\Test\BuildTest.ps1
```

## 维护说明

### 脚本管理
- 新的构建相关脚本应放在 `Scripts/Build/`
- 测试脚本应放在 `Scripts/Test/`
- 部署脚本应放在 `Scripts/Deploy/`
- 修改脚本时请更新相应的文档

### 文档维护
- 实施总结文档保存在 `Docs/implementation/`
- 技术分析文档保存在 `Docs/technical/`
- 项目文档保存在 `Docs/project/`

### 版本管理
- 使用 Git 子模块管理 whisper.cpp 版本
- 定期更新子模块到最新稳定版本
- 记录重要的版本变更和兼容性问题

## 历史记录

- 2025-06-24: 完成 whisper.cpp 升级和构建系统重构
- 2025-06-24: 实施脚本统一管理和项目结构整理
- 2025-06-24: 清理废弃项目和临时文件
