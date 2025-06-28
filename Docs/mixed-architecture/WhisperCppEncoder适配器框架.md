```
/*
 * WhisperCppEncoder.h - 适配器接口
 * 这个类的作用是作为新旧代码之间的桥梁。
 */
#pragma once

#include "CWhisperEngine.h" // 包含我们自己的引擎
#include "API/iSpectrogram.h" // 包含原项目的频谱图接口
#include "API/iTranscribeResult.h" // 包含原项目的结果接口

class WhisperCppEncoder {
public:
    // 构造函数：接收模型路径，并在内部创建CWhisperEngine
    WhisperCppEncoder(const std::string& modelPath);

    // 核心的 "encode" 方法
    // 它将被 ContextImpl 调用
    // HRESULT 是Windows常用的返回类型，表示成功或失败
    HRESULT encode(
        iSpectrogram& spectrogram,      // 输入：原项目的频谱图对象
        iTranscribeResult& resultSink   // 输出：用于存放结果的对象
    );

private:
    // 持有我们的新引擎实例
    CWhisperEngine m_engine; 
};


/*
 * WhisperCppEncoder.cpp - 适配器实现
 */

#include "WhisperCppEncoder.h"
#include <vector>

// 构造函数实现
WhisperCppEncoder::WhisperCppEncoder(const std::string& modelPath)
    : m_engine(modelPath) // 使用成员初始化列表创建引擎
{
    // 构造函数体可以为空
}

// 核心的适配/转换逻辑
HRESULT WhisperCppEncoder::encode(iSpectrogram& spectrogram, iTranscribeResult& resultSink)
{
    try
    {
        // 1. 数据转换：从 iSpectrogram -> std::vector<float>
        // 这是最关键的一步，需要从 spectrogram 中提取出完整的MEL数据
        // 注意：原项目的 iSpectrogram 可能设计为逐块读取，
        // 而我们的 CWhisperEngine::transcribe 需要完整数据。
        // 我们需要将所有块拼接起来。
        const size_t melLength = spectrogram.getLength();
        // 假设每个时间步有80个MEL频带 (N_MEL)
        const size_t totalFloats = melLength * 80; 
        std::vector<float> audioFeatures(totalFloats);

        const float* pBuffer = nullptr;
        size_t stride = 0;
        // 调用 makeBuffer 获取完整的频谱图数据
        HRESULT hr = spectrogram.makeBuffer(0, melLength, &pBuffer, stride);
        if (FAILED(hr)) {
            return hr; // 如果提取失败，直接返回错误码
        }
        // 拷贝数据到我们的vector中
        memcpy(audioFeatures.data(), pBuffer, totalFloats * sizeof(float));


        // 2. 调用新引擎的核心转录方法
        TranscriptionParams params; // 使用默认参数
        TranscriptionResult engineResult = m_engine.transcribe(audioFeatures, params);


        // 3. 结果转换：从 TranscriptionResult -> iTranscribeResult
        // 遍历我们引擎返回的结果，并将其添加到 resultSink 中
        if (engineResult.success) {
            for (const auto& segment : engineResult.segments) {
                // 调用 resultSink 的方法来添加分段
                // 具体的函数名需要查看 iTranscribeResult.h 的定义
                // 例如：resultSink.addSegment(segment.text, segment.start_ms, segment.end_ms);
            }
        } else {
            // 处理转录失败的情况
            return E_FAIL; 
        }

        return S_OK; // 返回成功
    }
    catch (const CWhisperError& e)
    {
        // 捕获我们引擎的异常，并转换为HRESULT错误码
        // Log the error message e.what()
        return E_FAIL;
    }
    catch (...)
    {
        // 捕获其他未知异常
        return E_UNEXPECTED;
    }
}

```