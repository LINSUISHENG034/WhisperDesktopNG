# WhisperDesktopNG 构建脚本目录

本目录包含了项目的所有构建和部署相关脚本，统一管理以便维护。

## 目录结构

```
Scripts/
├── README.md                    # 本说明文件
├── Build/                       # 构建相关脚本
│   ├── build_whisper.bat       # Whisper.cpp 构建脚本（推荐使用）
│   ├── build_whisper_static.bat # Whisper.cpp 静态库构建脚本（备用）
│   ├── clean_build.bat         # 构建清理脚本
│   ├── CMakeLists.txt          # CMake 配置文件（已移至 whisper_build/）
│   └── whisper_build/          # Whisper.cpp 构建工作目录
│       ├── CMakeLists.txt      # CMake 配置文件
│       └── build/              # 构建输出目录（自动生成）
├── Test/                        # 测试相关脚本
│   ├── BuildTest.ps1           # 构建验证测试
│   └── SimpleTest.ps1          # 简单功能测试
└── Deploy/                      # 部署相关脚本
    └── copy-binaries.cmd       # 二进制文件复制脚本
```

## 脚本说明

### 构建脚本 (Build/)

#### build_whisper.bat
- **用途**: 编译最新版本的 whisper.cpp 为静态库
- **功能**: 
  - 自动检测 whisper.cpp 子模块
  - 使用 CMake 配置和编译
  - 生成 whisper.lib, ggml.lib 等静态库
  - 自动复制库文件和头文件到项目目录
- **使用方法**: 从项目根目录运行 `Scripts\Build\build_whisper.bat`

#### build_whisper_static.bat
- **用途**: 备用的 whisper.cpp 构建脚本
- **状态**: 包含更详细的配置选项，但可能存在兼容性问题
- **建议**: 优先使用 build_whisper.bat

#### CMakeLists.txt
- **用途**: whisper.cpp 的 CMake 配置文件
- **功能**: 配置静态库编译选项、优化设置等
- **位置**: 配合构建脚本使用，无需单独运行

### 测试脚本 (Test/)

#### BuildTest.ps1
- **用途**: 验证项目构建是否成功
- **功能**: 检查编译输出、库文件完整性等
- **使用方法**: `powershell Scripts\Test\BuildTest.ps1`

#### SimpleTest.ps1
- **用途**: 基本功能测试
- **功能**: 测试基本的 API 调用和功能
- **使用方法**: `powershell Scripts\Test\SimpleTest.ps1`

### 部署脚本 (Deploy/)

#### copy-binaries.cmd
- **用途**: 复制编译好的二进制文件到部署目录
- **功能**: 自动化部署流程
- **使用方法**: `Scripts\Deploy\copy-binaries.cmd`

## 使用指南

### 首次构建
1. 确保已安装 CMake (路径: E:\Program Files\CMake)
2. 确保 Git 子模块已初始化: `git submodule update --init --recursive`
3. 从项目根目录运行构建脚本: `Scripts\Build\build_whisper.bat`

### 日常开发
1. 修改代码后重新构建: `Scripts\Build\build_whisper.bat`
2. 运行测试验证: `Scripts\Test\BuildTest.ps1`
3. 部署到目标目录: `Scripts\Deploy\copy-binaries.cmd`

### 故障排除
1. 如果 CMake 路径不同，请修改构建脚本中的 CMake 路径
2. 如果遇到权限问题，请以管理员身份运行脚本
3. 如果子模块未初始化，请先运行 `git submodule update --init --recursive`

## 维护说明

- 所有新的构建相关脚本应放在此目录下
- 修改脚本时请更新相应的说明文档
- 测试脚本应保持独立，不依赖外部环境
- 部署脚本应支持不同的目标环境

## 历史记录

- 2025-06-24: 创建统一的脚本目录结构
- 2025-06-24: 迁移现有构建脚本到新目录
- 2025-06-24: 添加 whisper.cpp 构建支持
