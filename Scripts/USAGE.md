# 构建脚本使用指南

## 快速开始

### 推荐方式：从项目根目录调用
```bash
# 从项目根目录直接调用构建脚本
Scripts\Build\build_whisper.bat
```

### 或者进入脚本目录
```bash
# 进入脚本目录
cd Scripts\Build

# 运行构建脚本
.\build_whisper.bat
```

## 常用操作

### 1. 首次构建
```bash
# 确保子模块已初始化
git submodule update --init --recursive

# 运行构建
Scripts\Build\build_whisper.bat
```

### 2. 清理构建
```bash
# 清理所有构建文件
Scripts\Build\clean_build.bat

# 重新构建
Scripts\Build\build_whisper.bat
```

### 3. 运行测试
```bash
# 构建验证测试
powershell Scripts\Test\BuildTest.ps1

# 简单功能测试
powershell Scripts\Test\SimpleTest.ps1
```

### 4. 部署文件
```bash
# 复制二进制文件到部署目录
Scripts\Deploy\copy-binaries.cmd
```

## 故障排除

### CMake 路径问题
如果 CMake 安装在不同位置，请修改 `Scripts\Build\build_whisper.bat` 中的路径：
```batch
"E:\Program Files\CMake\bin\cmake.exe"
```
改为您的 CMake 安装路径。

### 权限问题
如果遇到权限错误，请以管理员身份运行命令提示符。

### 子模块问题
如果提示找不到 whisper.cpp 子模块：
```bash
git submodule update --init --recursive
```

## 输出文件

构建成功后，将生成以下文件：

### 静态库文件
- `lib\x64\Release\whisper.lib` - 主要语音识别库
- `lib\x64\Release\ggml.lib` - 机器学习基础库
- `lib\x64\Release\ggml-base.lib` - GGML 基础组件
- `lib\x64\Release\ggml-cpu.lib` - CPU 优化后端

### 头文件
- `include\whisper_cpp\whisper.h` - 主要头文件
- `include\whisper_cpp\ggml.h` - GGML 头文件
- 其他相关头文件

## 脚本说明

### Build/ 目录
- `build_whisper.bat` - 主要构建脚本（推荐）
- `build_whisper_static.bat` - 备用构建脚本
- `clean_build.bat` - 清理脚本
- `CMakeLists.txt` - CMake 配置

### Test/ 目录
- `BuildTest.ps1` - 构建验证测试
- `SimpleTest.ps1` - 基本功能测试

### Deploy/ 目录
- `copy-binaries.cmd` - 部署脚本

## 注意事项

1. 确保已安装 CMake 4.0+ 版本
2. 确保已安装 Visual Studio 2022 或更高版本
3. 首次使用前请初始化 Git 子模块
4. 构建过程可能产生警告，但不影响功能
5. 如需修改编译选项，请编辑 `CMakeLists.txt` 文件
