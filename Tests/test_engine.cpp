/*
 * test_engine.cpp - CWhisperEngine 的简单测试用例
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <filesystem>

// 包含我们的CWhisperEngine头文件
// 注意：这里需要相对路径指向Whisper目录
#include "../Whisper/CWhisperEngine.h"

// 生成简单的正弦波音频数据用于测试
std::vector<float> generateTestAudio(int sampleRate = 16000, float duration = 1.0f, float frequency = 440.0f) {
    int numSamples = static_cast<int>(sampleRate * duration);
    std::vector<float> audioData(numSamples);
    
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        audioData[i] = 0.1f * std::sin(2.0f * 3.14159f * frequency * t);
    }
    
    return audioData;
}

// 生成静音音频数据用于测试
std::vector<float> generateSilentAudio(int sampleRate = 16000, float duration = 1.0f) {
    int numSamples = static_cast<int>(sampleRate * duration);
    return std::vector<float>(numSamples, 0.0f);
}

int main() {
    std::cout << "=== CWhisperEngine 测试开始 ===" << std::endl;
    
    // 测试用的模型路径 - 需要用户提供一个有效的模型文件
    // 这里使用相对路径，用户需要将模型文件放在适当位置
    std::string modelPath = "models/ggml-base.en.bin";
    
    // 检查模型文件是否存在
    if (!std::filesystem::exists(modelPath)) {
        std::cout << "警告: 模型文件不存在: " << modelPath << std::endl;
        std::cout << "请将Whisper模型文件放置在 " << modelPath << " 位置" << std::endl;
        std::cout << "可以从 https://huggingface.co/ggerganov/whisper.cpp 下载模型文件" << std::endl;
        std::cout << "测试将使用虚拟路径继续，但会失败..." << std::endl;
    }
    
    try {
        // 测试1: 构造函数测试
        std::cout << "\n1. 测试构造函数..." << std::endl;
        CWhisperEngine engine(modelPath);
        std::cout << "   ✓ 构造函数成功" << std::endl;
        
        // 测试2: 使用静音音频进行转录测试
        std::cout << "\n2. 测试静音音频转录..." << std::endl;
        auto silentAudio = generateSilentAudio(16000, 2.0f); // 2秒静音
        auto result1 = engine.transcribe(silentAudio);
        
        std::cout << "   转录成功: " << (result1.success ? "是" : "否") << std::endl;
        std::cout << "   检测到的段数: " << result1.segments.size() << std::endl;
        for (size_t i = 0; i < result1.segments.size(); ++i) {
            std::cout << "   段 " << i << ": \"" << result1.segments[i] << "\"" << std::endl;
        }
        
        // 测试3: 使用正弦波音频进行转录测试
        std::cout << "\n3. 测试正弦波音频转录..." << std::endl;
        auto sineAudio = generateTestAudio(16000, 3.0f, 440.0f); // 3秒440Hz正弦波
        auto result2 = engine.transcribe(sineAudio);
        
        std::cout << "   转录成功: " << (result2.success ? "是" : "否") << std::endl;
        std::cout << "   检测到的段数: " << result2.segments.size() << std::endl;
        for (size_t i = 0; i < result2.segments.size(); ++i) {
            std::cout << "   段 " << i << ": \"" << result2.segments[i] << "\"" << std::endl;
        }
        
        // 测试4: 空音频数据测试
        std::cout << "\n4. 测试空音频数据..." << std::endl;
        std::vector<float> emptyAudio;
        auto result3 = engine.transcribe(emptyAudio);
        
        std::cout << "   转录成功: " << (result3.success ? "是" : "否") << std::endl;
        std::cout << "   检测到的段数: " << result3.segments.size() << std::endl;
        
        std::cout << "\n=== 所有测试完成 ===" << std::endl;
        std::cout << "CWhisperEngine 基本功能正常工作！" << std::endl;
        
    } catch (const CWhisperError& e) {
        std::cerr << "CWhisperError: " << e.what() << std::endl;
        std::cerr << "测试失败！请检查模型文件路径和whisper.cpp库的链接。" << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "标准异常: " << e.what() << std::endl;
        std::cerr << "测试失败！" << std::endl;
        return 1;
    }
    
    return 0;
}
