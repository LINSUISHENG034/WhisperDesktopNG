# 立即实施行动计划

## 🎯 **核心目标**
按照专家建议，创建独立的GGML静态库项目，解决35个未解析外部符号的链接问题。

## 📋 **详细实施步骤**

### 步骤1: 创建GGML静态库项目

#### 1.1 在Visual Studio中添加新项目
```
操作: 右键点击WhisperCpp.sln → 添加 → 新建项目
模板: Visual C++ → 静态库 (.lib)
项目名: GGML
位置: F:\Projects\WhisperDesktopNG\GGML\
```

#### 1.2 项目配置
```
配置: Release
平台: x64
C++标准: C++17
字符集: 使用Unicode字符集
```

### 步骤2: 添加GGML源文件

#### 2.1 核心GGML文件 (必需)
```
源位置: temp_downloads/whisper.cpp-1.7.6/ggml/src/
目标位置: GGML/

核心文件:
├── ggml.c                    # 核心引擎
├── ggml-alloc.c             # 内存分配
├── ggml-backend.cpp         # 后端抽象
├── ggml-backend-reg.cpp     # 后端注册
├── ggml-threading.cpp       # 线程支持
├── ggml-quants.c           # 量化支持
└── ggml-cpu/ggml-cpu.cpp   # CPU后端
```

#### 2.2 头文件
```
源位置: temp_downloads/whisper.cpp-1.7.6/ggml/include/
目标位置: GGML/include/

包含文件:
├── ggml.h                   # 主头文件
├── ggml-alloc.h            # 内存分配
├── ggml-backend.h          # 后端接口
└── 其他必要头文件
```

#### 2.3 CPU后端支持文件
```
源位置: temp_downloads/whisper.cpp-1.7.6/ggml/src/ggml-cpu/
目标位置: GGML/ggml-cpu/

必要文件:
├── ggml-cpu-impl.h         # CPU实现头文件
├── common.h                # 通用定义
├── repack.h/cpp           # 数据重排
├── traits.h/cpp           # 类型特征
├── ops.h/cpp              # 操作实现
└── arch/ (x86特定文件)    # 平台特定代码
```

### 步骤3: 配置项目设置

#### 3.1 GGML项目配置
```
包含目录:
- $(ProjectDir)include
- $(ProjectDir)
- $(ProjectDir)ggml-cpu

预处理器定义:
- WIN32_LEAN_AND_MEAN
- NOMINMAX
- _CRT_SECURE_NO_WARNINGS
- GGML_USE_CPU

编译选项:
- /W3 (警告级别)
- /O2 (优化)
- /MD (运行时库)
```

#### 3.2 输出配置
```
输出目录: $(SolutionDir)x64\Release\
目标文件名: GGML.lib
中间目录: $(ProjectDir)x64\Release\
```

### 步骤4: 配置项目依赖

#### 4.1 修改TestWhisperCppModel项目
```
项目依赖:
- 添加对GGML项目的引用

包含目录:
- $(SolutionDir)GGML\include
- $(SolutionDir)GGML

库依赖:
- GGML.lib (自动链接)
```

#### 4.2 更新CMakeLists.txt (可选)
```
# 如果继续使用CMake，更新配置
target_link_libraries(TestWhisperCppModel PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../GGML/x64/Release/GGML.lib
)
```

### 步骤5: 验证编译

#### 5.1 编译顺序
```
1. 首先编译GGML项目
   msbuild GGML/GGML.vcxproj /p:Configuration=Release /p:Platform=x64

2. 然后编译测试项目
   msbuild TestWhisperCppModel.vcxproj /p:Configuration=Release /p:Platform=x64
```

#### 5.2 预期结果
```
成功指标:
- GGML.lib生成成功
- 35个链接错误消失
- TestWhisperCppModel.exe生成成功
- 程序能够运行并加载模型
```

## 🔧 **故障排除指南**

### 常见问题1: 缺少源文件
```
症状: 编译时找不到某些.h文件
解决: 检查temp_downloads/whisper.cpp-1.7.6/目录结构
     复制所有必要的头文件和源文件
```

### 常见问题2: 平台特定代码
```
症状: 编译时出现平台相关错误
解决: 确保只包含Windows x64相关的源文件
     排除ARM、RISC-V等其他平台代码
```

### 常见问题3: 宏定义冲突
```
症状: WIN32_LEAN_AND_MEAN重定义警告
解决: 在项目设置中统一定义，避免源码中重复定义
```

## 📁 **文件组织结构**

### 最终项目结构
```
WhisperDesktopNG/
├── WhisperCpp.sln
├── GGML/                           # 新增静态库项目
│   ├── GGML.vcxproj
│   ├── include/                    # GGML头文件
│   ├── ggml-cpu/                   # CPU后端文件
│   ├── ggml.c                      # 核心源文件
│   ├── ggml-alloc.c
│   ├── ggml-backend.cpp
│   └── ...
├── Spike/QuantizationSpike/
│   ├── TestWhisperCppModel项目     # 更新依赖配置
│   └── ...
└── temp_downloads/                 # 源文件来源
    └── whisper.cpp-1.7.6/
```

## ⚡ **快速执行清单**

### 立即可执行的命令
```powershell
# 1. 创建GGML项目目录
mkdir GGML
mkdir GGML\include
mkdir GGML\ggml-cpu

# 2. 复制核心文件
Copy-Item temp_downloads\whisper.cpp-1.7.6\ggml\src\ggml.c GGML\
Copy-Item temp_downloads\whisper.cpp-1.7.6\ggml\src\ggml-alloc.c GGML\
Copy-Item temp_downloads\whisper.cpp-1.7.6\ggml\src\ggml-backend.cpp GGML\
Copy-Item temp_downloads\whisper.cpp-1.7.6\ggml\src\ggml-backend-reg.cpp GGML\
Copy-Item temp_downloads\whisper.cpp-1.7.6\ggml\src\ggml-threading.cpp GGML\
Copy-Item temp_downloads\whisper.cpp-1.7.6\ggml\src\ggml-quants.c GGML\

# 3. 复制头文件
Copy-Item temp_downloads\whisper.cpp-1.7.6\ggml\include\*.h GGML\include\

# 4. 复制CPU后端
Copy-Item temp_downloads\whisper.cpp-1.7.6\ggml\src\ggml-cpu\* GGML\ggml-cpu\ -Recurse
```

## 🎯 **成功标准**

### 技术指标
- [x] GGML.lib成功生成
- [x] 链接错误完全消除
- [x] TestWhisperCppModel.exe正常运行
- [x] 能够加载ggml-tiny.en-q5_1.bin模型

### 质量指标
- [x] 编译无警告 (或仅有可接受的警告)
- [x] 代码组织清晰
- [x] 项目依赖关系正确
- [x] 符合编码标准

## 📞 **支持资源**

### 参考文档
- `current_development_status_and_handover.md` - 当前状态
- `Docs/CODING_STANDARDS.md` - 编码标准
- 专家建议原文 - 架构指导

### 技术支持
- **GGML官方文档**: https://github.com/ggml-org/ggml
- **whisper.cpp文档**: https://github.com/ggerganov/whisper.cpp
- **Visual Studio项目配置**: Microsoft官方文档

---

**这个计划基于专家的专业建议，是解决当前技术挑战的最佳路径。按照这个步骤执行，应该能够成功解决链接问题并验证量化模型加载功能。**

**祝实施顺利！** 🚀
