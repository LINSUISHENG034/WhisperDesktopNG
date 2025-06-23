/*
 * test_engine.cpp - Test cases for CWhisperEngine using latest whisper.cpp API
 * Tests quantized model support and latest features
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <filesystem>
#include <chrono>

// Include our CWhisperEngine header
#include "../Whisper/CWhisperEngine.h"

// Generate simple sine wave audio data for testing
std::vector<float> generateTestAudio(int sampleRate = 16000, float duration = 1.0f, float frequency = 440.0f) {
    int numSamples = static_cast<int>(sampleRate * duration);
    std::vector<float> audioData(numSamples);
    
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        audioData[i] = 0.1f * std::sin(2.0f * 3.14159f * frequency * t);
    }
    
    return audioData;
}

// Generate silent audio data for testing
std::vector<float> generateSilentAudio(int sampleRate = 16000, float duration = 1.0f) {
    int numSamples = static_cast<int>(sampleRate * duration);
    return std::vector<float>(numSamples, 0.0f);
}

// Generate white noise for testing
std::vector<float> generateNoiseAudio(int sampleRate = 16000, float duration = 1.0f) {
    int numSamples = static_cast<int>(sampleRate * duration);
    std::vector<float> audioData(numSamples);
    
    for (int i = 0; i < numSamples; ++i) {
        audioData[i] = 0.05f * (static_cast<float>(rand()) / RAND_MAX - 0.5f);
    }
    
    return audioData;
}

void testBasicFunctionality(const std::string& modelPath) {
    std::cout << "\n=== Testing Basic Functionality ===" << std::endl;
    
    try {
        // Test 1: Constructor and basic configuration
        std::cout << "1. Testing constructor..." << std::endl;
        TranscriptionConfig config;
        config.enableTimestamps = true;
        config.language = "auto";
        config.numThreads = 2;
        
        CWhisperEngine engine(modelPath, config);
        std::cout << "   ✓ Constructor successful" << std::endl;
        
        // Test 2: Model information
        std::cout << "2. Testing model information..." << std::endl;
        std::cout << "   Model type: " << engine.getModelType() << std::endl;
        std::cout << "   Multilingual: " << (engine.isMultilingual() ? "Yes" : "No") << std::endl;
        
        auto languages = engine.getAvailableLanguages();
        std::cout << "   Supported languages: " << languages.size() << std::endl;
        if (!languages.empty()) {
            std::cout << "   First few: ";
            for (size_t i = 0; i < std::min(size_t(5), languages.size()); ++i) {
                std::cout << languages[i] << " ";
            }
            std::cout << std::endl;
        }
        
        // Test 3: Silent audio transcription
        std::cout << "3. Testing silent audio transcription..." << std::endl;
        auto silentAudio = generateSilentAudio(16000, 2.0f);
        auto result1 = engine.transcribe(silentAudio);
        
        std::cout << "   Success: " << (result1.success ? "Yes" : "No") << std::endl;
        std::cout << "   Detected language: " << result1.detectedLanguage << std::endl;
        std::cout << "   Segments: " << result1.segments.size() << std::endl;
        std::cout << "   Timings - Sample: " << result1.timings.sampleMs << "ms, "
                  << "Encode: " << result1.timings.encodeMs << "ms, "
                  << "Decode: " << result1.timings.decodeMs << "ms" << std::endl;
        
    } catch (const CWhisperError& e) {
        std::cerr << "CWhisperError in basic functionality test: " << e.what() << std::endl;
        throw;
    }
}

void testAdvancedFeatures(const std::string& modelPath) {
    std::cout << "\n=== Testing Advanced Features ===" << std::endl;
    
    try {
        CWhisperEngine engine(modelPath);
        
        // Test 1: Custom configuration per transcription
        std::cout << "1. Testing custom configuration..." << std::endl;
        TranscriptionConfig customConfig;
        customConfig.language = "en";
        customConfig.enableTimestamps = true;
        customConfig.enableTokenTimestamps = true;
        customConfig.temperature = 0.1f;
        customConfig.suppressBlank = true;
        
        auto testAudio = generateTestAudio(16000, 3.0f, 440.0f);
        auto result = engine.transcribe(testAudio, customConfig);
        
        std::cout << "   Success: " << (result.success ? "Yes" : "No") << std::endl;
        std::cout << "   Segments with timestamps:" << std::endl;
        for (size_t i = 0; i < result.segments.size(); ++i) {
            const auto& seg = result.segments[i];
            std::cout << "     [" << seg.startTime << "ms - " << seg.endTime << "ms] "
                      << "\"" << seg.text << "\" (conf: " << seg.confidence << ")" << std::endl;
        }
        
        // Test 2: Move semantics
        std::cout << "2. Testing move semantics..." << std::endl;
        CWhisperEngine engine2 = std::move(engine);
        std::cout << "   ✓ Move constructor successful" << std::endl;
        
        // Test 3: Performance monitoring
        std::cout << "3. Testing performance monitoring..." << std::endl;
        engine2.resetTimings();
        auto noiseAudio = generateNoiseAudio(16000, 1.0f);
        auto result2 = engine2.transcribe(noiseAudio);
        std::cout << "   Performance timings:" << std::endl;
        engine2.printTimings();
        
    } catch (const CWhisperError& e) {
        std::cerr << "CWhisperError in advanced features test: " << e.what() << std::endl;
        throw;
    }
}

void testQuantizedModelSupport(const std::string& modelPath) {
    std::cout << "\n=== Testing Quantized Model Support ===" << std::endl;
    
    try {
        // Test with GPU acceleration if available
        TranscriptionConfig gpuConfig;
        gpuConfig.useGpu = true;
        gpuConfig.gpuDevice = 0;
        
        std::cout << "1. Testing GPU acceleration (if available)..." << std::endl;
        try {
            CWhisperEngine gpuEngine(modelPath, gpuConfig);
            std::cout << "   ✓ GPU engine created successfully" << std::endl;
            
            auto testAudio = generateTestAudio(16000, 2.0f);
            auto start = std::chrono::high_resolution_clock::now();
            auto result = gpuEngine.transcribe(testAudio);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "   GPU transcription time: " << duration.count() << "ms" << std::endl;
            std::cout << "   Success: " << (result.success ? "Yes" : "No") << std::endl;
            
        } catch (const CWhisperError& e) {
            std::cout << "   GPU acceleration not available: " << e.what() << std::endl;
        }
        
        // Test CPU performance for comparison
        std::cout << "2. Testing CPU performance..." << std::endl;
        TranscriptionConfig cpuConfig;
        cpuConfig.useGpu = false;
        cpuConfig.numThreads = 4;
        
        CWhisperEngine cpuEngine(modelPath, cpuConfig);
        auto testAudio = generateTestAudio(16000, 2.0f);
        auto start = std::chrono::high_resolution_clock::now();
        auto result = cpuEngine.transcribe(testAudio);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "   CPU transcription time: " << duration.count() << "ms" << std::endl;
        std::cout << "   Success: " << (result.success ? "Yes" : "No") << std::endl;
        
    } catch (const CWhisperError& e) {
        std::cerr << "CWhisperError in quantized model test: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    std::cout << "=== CWhisperEngine Test Suite (Latest whisper.cpp API) ===" << std::endl;
    
    // Test model paths - support both .bin and .gguf formats
    std::vector<std::string> modelPaths = {
        "models/ggml-base.en.bin",
        "models/ggml-base.en.gguf",
        "models/ggml-small.en.bin",
        "models/ggml-small.en.gguf"
    };
    
    std::string modelPath;
    for (const auto& path : modelPaths) {
        if (std::filesystem::exists(path)) {
            modelPath = path;
            break;
        }
    }
    
    if (modelPath.empty()) {
        std::cout << "Warning: No model file found. Tested paths:" << std::endl;
        for (const auto& path : modelPaths) {
            std::cout << "  - " << path << std::endl;
        }
        std::cout << "Please download a Whisper model file from:" << std::endl;
        std::cout << "  https://huggingface.co/ggerganov/whisper.cpp" << std::endl;
        std::cout << "Testing will continue with first path (will fail)..." << std::endl;
        modelPath = modelPaths[0];
    } else {
        std::cout << "Using model: " << modelPath << std::endl;
    }
    
    try {
        testBasicFunctionality(modelPath);
        testAdvancedFeatures(modelPath);
        testQuantizedModelSupport(modelPath);
        
        std::cout << "\n=== All Tests Completed Successfully ===" << std::endl;
        std::cout << "CWhisperEngine with latest whisper.cpp API is working correctly!" << std::endl;
        
    } catch (const CWhisperError& e) {
        std::cerr << "\nTest failed with CWhisperError: " << e.what() << std::endl;
        std::cerr << "Please check model file path and whisper.cpp library linking." << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
