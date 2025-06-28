```
/*
 * CWhisperEngine.h - 现代C++封装 whisper.cpp 的接口
 */
#pragma once

#include <string>
#include <vector>
#include <stdexcept>

// 向前声明C API中的结构体，避免在头文件中暴露底层细节
struct whisper_context;
struct whisper_full_params;

// 定义一个清晰的结构体来保存转录结果
struct TranscriptionResult {
    bool success = false;
    std::string detectedLanguage;
    std::vector<std::string> segments;
    // 未来可以添加更多信息，如时间戳
};

// CWhisperEngine 类声明
class CWhisperEngine {
public:
    // 构造函数：加载模型，失败则抛出异常
    explicit CWhisperEngine(const std::string& modelPath);

    // 析构函数：自动释放模型资源
    ~CWhisperEngine();

    // 核心转录方法：接收音频数据，返回转录结果
    TranscriptionResult transcribe(const std::vector<float>& audioData);

    // 禁用拷贝构造和拷贝赋值，防止资源管理混乱
    CWhisperEngine(const CWhisperEngine&) = delete;
    CWhisperEngine& operator=(const CWhisperEngine&) = delete;

private:
    whisper_context* m_ctx = nullptr;
};

// 自定义异常类，用于报告引擎相关的错误
class CWhisperError : public std::runtime_error {
public:
    explicit CWhisperError(const std::string& message) : std::runtime_error(message) {}
};


/*
 * CWhisperEngine.cpp - CWhisperEngine 的实现
 */

// 只在 .cpp 文件中包含底层C API头文件
extern "C" {
#include "whisper.h"
}

#include <thread> // for std::thread::hardware_concurrency

CWhisperEngine::CWhisperEngine(const std::string& modelPath) {
    // 调用导出的C API函数来初始化上下文
    // 注意：这里我们假设导出的函数名与原始函数名一致
    // 如果您在.def文件中使用了不同的导出名，请在此处修改
    m_ctx = whisper_init_from_file(modelPath.c_str());
    if (m_ctx == nullptr) {
        throw CWhisperError("Failed to initialize whisper context from model file: " + modelPath);
    }
}

CWhisperEngine::~CWhisperEngine() {
    if (m_ctx) {
        whisper_free(m_ctx);
    }
}

TranscriptionResult CWhisperEngine::transcribe(const std::vector<float>& audioData) {
    if (!m_ctx) {
        throw CWhisperError("Whisper context is not initialized.");
    }

    // 1. 设置转录参数
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads = std::max(1, (int)std::thread::hardware_concurrency() / 2);
    params.print_progress = false; // 我们不需要库自己打印进度
    
    // 2. 调用核心转录函数
    if (whisper_full(m_ctx, params, audioData.data(), audioData.size()) != 0) {
        throw CWhisperError("Failed to process audio data with whisper_full.");
    }

    // 3. 提取结果
    TranscriptionResult result;
    const int n_segments = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(m_ctx, i);
        result.segments.push_back(text);
    }
    
    result.success = true;
    // (可选) 获取检测到的语言
    // result.detectedLanguage = ...;

    return result;
}

```