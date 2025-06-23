# Git Commit 标准规范

## 概述

本文档定义了WhisperDesktopNG项目的Git提交消息标准，旨在确保提交历史的清晰性、可读性和可维护性。

## Commit Message 格式

### 基本结构

```
<type>(<scope>): <subject>

<body>

<footer>
```

### 详细说明

#### 1. Type (类型) - 必需

提交的类型，必须是以下之一：

- **feat**: 新功能 (feature)
- **fix**: 修复bug
- **docs**: 文档变更
- **style**: 代码格式变更（不影响代码逻辑）
- **refactor**: 重构（既不是新功能也不是修复bug）
- **perf**: 性能优化
- **test**: 添加或修改测试
- **build**: 构建系统或外部依赖变更
- **ci**: CI配置文件和脚本变更
- **chore**: 其他不修改src或test文件的变更
- **revert**: 回滚之前的提交

#### 2. Scope (范围) - 可选

影响的模块或组件，例如：

- **core**: 核心Whisper库
- **ui**: 用户界面
- **build**: 构建系统
- **docs**: 文档
- **tests**: 测试
- **tools**: 工具和脚本
- **config**: 配置文件

#### 3. Subject (主题) - 必需

- 使用现在时态的动词开头
- 首字母小写
- 结尾不加句号
- 限制在50个字符以内
- 简洁明了地描述变更内容

#### 4. Body (正文) - 可选

- 详细描述变更的原因和内容
- 使用现在时态
- 每行限制在72个字符以内
- 与subject之间空一行

#### 5. Footer (页脚) - 可选

- 引用相关的issue或PR
- 描述破坏性变更
- 格式：`Closes #123` 或 `Fixes #456`

## 示例

### 基本示例

```bash
feat(ui): add voice recording button

Add a new recording button to the main interface that allows users 
to start and stop voice recording with visual feedback.

Closes #42
```

### 修复bug示例

```bash
fix(core): resolve memory leak in audio processing

Fixed memory leak that occurred when processing long audio files.
The issue was caused by improper cleanup of audio buffers.

Fixes #89
```

### 文档更新示例

```bash
docs(readme): update installation instructions

- Add Windows 11 compatibility notes
- Update Visual Studio version requirements
- Fix broken links to dependencies
```

### 构建系统示例

```bash
build(msbuild): update project configuration for Release mode

- Enable link-time code generation
- Optimize for size instead of speed
- Update include paths for better compatibility
```

## 最佳实践

### DO (推荐做法)

1. **保持简洁**: 主题行简洁明了
2. **使用现在时**: "add feature" 而不是 "added feature"
3. **说明原因**: 在正文中解释为什么做这个变更
4. **引用issue**: 使用footer引用相关的issue或PR
5. **逻辑分组**: 相关的变更放在一个commit中
6. **测试验证**: 确保每个commit都是可编译和可测试的

### DON'T (避免做法)

1. **避免无意义的消息**: 如 "fix", "update", "changes"
2. **避免过长的主题**: 超过50个字符
3. **避免混合变更**: 一个commit包含多个不相关的变更
4. **避免破坏构建**: 每个commit都应该保持项目可编译
5. **避免敏感信息**: 不要在commit消息中包含密码或密钥

## 工具和自动化

### Pre-commit Hook

建议使用pre-commit hook来验证commit消息格式：

```bash
#!/bin/sh
# .git/hooks/commit-msg

commit_regex='^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\(.+\))?: .{1,50}'

if ! grep -qE "$commit_regex" "$1"; then
    echo "Invalid commit message format!"
    echo "Please use: <type>(<scope>): <subject>"
    exit 1
fi
```

### 推荐工具

- **Commitizen**: 交互式commit消息生成
- **Conventional Changelog**: 自动生成变更日志
- **Husky**: Git hooks管理

## 分支命名规范

### 分支类型

- **feature/**: 新功能分支 (feature/voice-recording)
- **fix/**: 修复分支 (fix/memory-leak)
- **docs/**: 文档分支 (docs/api-documentation)
- **refactor/**: 重构分支 (refactor/audio-processing)
- **test/**: 测试分支 (test/unit-tests)

### 命名格式

```
<type>/<short-description>
```

例如：
- `feature/add-translation-support`
- `fix/resolve-crash-on-startup`
- `docs/update-build-instructions`

## 版本标签

### 语义化版本

使用语义化版本控制 (SemVer)：

```
MAJOR.MINOR.PATCH
```

- **MAJOR**: 不兼容的API变更
- **MINOR**: 向后兼容的功能新增
- **PATCH**: 向后兼容的问题修复

### 标签格式

```bash
git tag -a v1.2.3 -m "Release version 1.2.3"
```

## 检查清单

提交前请确认：

- [ ] Commit消息遵循标准格式
- [ ] 主题行少于50个字符
- [ ] 如有必要，包含详细的正文说明
- [ ] 引用了相关的issue或PR
- [ ] 代码已经测试并且可以编译
- [ ] 没有包含敏感信息
- [ ] 变更是逻辑完整的

---

**文档版本**: 1.0  
**最后更新**: 2025年6月23日  
**适用项目**: WhisperDesktopNG
