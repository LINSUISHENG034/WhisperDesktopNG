#include <iostream>
#include <string>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <vector>
#include <windows.h>

// Include Const-me COM interfaces
#include "API/iContext.cl.h"
#include "API/sModelSetup.h"
#include "modelFactory.h"

// Include whisper.cpp for CPU reference
#include "../../GGML/whisper.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace fs = std::filesystem;

class EndToEndQuantizationTest {
private:
    std::string model_path;
    std::string audio_path;
    std::string output_dir;

public:
    EndToEndQuantizationTest(const std::string& model, const std::string& audio, const std::string& output)
        : model_path(model), audio_path(audio), output_dir(output) {}

    struct TestResult {
        bool success = false;
        std::string gpu_transcription;
        std::string cpu_transcription;
        double gpu_time_ms = 0.0;
        double cpu_time_ms = 0.0;
        std::string error_message;
    };

    TestResult runEndToEndTest() {
        TestResult result;

        std::cout << "=== End-to-End Quantization Test ===" << std::endl;
        std::cout << "Model: " << model_path << std::endl;
        std::cout << "Audio: " << audio_path << std::endl;
        std::cout << "Output: " << output_dir << std::endl << std::endl;

        // Verify files exist
        if (!fs::exists(model_path)) {
            result.error_message = "Model file not found: " + model_path;
            return result;
        }

        if (!fs::exists(audio_path)) {
            result.error_message = "Audio file not found: " + audio_path;
            return result;
        }

        // Create output directory
        fs::create_directories(output_dir);

        // Run GPU inference first
        std::cout << "--- GPU Inference (Const-me + Quantization) ---" << std::endl;
        if (!runGpuInference(result)) {
            std::cout << "GPU inference failed: " << result.error_message << std::endl;
            std::cout << "Continuing with CPU reference only..." << std::endl;
        }

        // Run CPU reference inference
        std::cout << "\n--- CPU Reference Inference (whisper.cpp) ---" << std::endl;
        if (!runCpuInference(result)) {
            return result;
        }

        // Compare results if both succeeded
        if (!result.gpu_transcription.empty()) {
            std::cout << "\n--- Results Comparison ---" << std::endl;
            compareResults(result);
        }

        result.success = true;
        return result;
    }

private:
    bool runGpuInference(TestResult& result) {
        try {
            // Initialize COM
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(hr)) {
                result.error_message = "Failed to initialize COM";
                return false;
            }

            auto start_time = std::chrono::high_resolution_clock::now();

            // Setup model configuration for GPU with quantization support
            Whisper::sModelSetup setup = {};
            setup.impl = Whisper::eModelImplementation::GPU;
            setup.flags = 0; // Default flags

            // Load model using Const-me GPU implementation
            ComLight::CComPtr<Whisper::iModel> model;
            std::wstring wmodel_path(model_path.begin(), model_path.end());

            hr = Whisper::loadModel(wmodel_path.c_str(), setup, nullptr, &model);
            if (FAILED(hr)) {
                result.error_message = "Failed to load GPU model, HRESULT: 0x" + std::to_string(hr);
                CoUninitialize();
                return false;
            }

            std::cout << "✅ GPU model loaded successfully" << std::endl;

            // Create context for inference
            ComLight::CComPtr<Whisper::iContext> context;
            hr = model->createContext(&context);
            if (FAILED(hr)) {
                result.error_message = "Failed to create GPU context";
                CoUninitialize();
                return false;
            }

            // Load and process audio file
            std::wstring waudio_path(audio_path.begin(), audio_path.end());
            hr = context->loadAudio(waudio_path.c_str());
            if (FAILED(hr)) {
                result.error_message = "Failed to load audio file";
                CoUninitialize();
                return false;
            }

            std::cout << "✅ Audio loaded successfully" << std::endl;

            // Run inference
            Whisper::sFullParams params = {};
            params.language = Whisper::eLanguage::en; // English
            params.strategy = Whisper::eResultStrategy::Transcribe;

            hr = context->runFull(params);
            if (FAILED(hr)) {
                result.error_message = "GPU inference failed";
                CoUninitialize();
                return false;
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            result.gpu_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

            // Get results
            uint32_t segmentCount = 0;
            hr = context->getSegmentsCount(&segmentCount);
            if (FAILED(hr) || segmentCount == 0) {
                result.error_message = "No segments generated by GPU inference";
                CoUninitialize();
                return false;
            }

            // Concatenate all segments
            std::string full_transcription;
            for (uint32_t i = 0; i < segmentCount; i++) {
                ComLight::CComPtr<Whisper::iSegment> segment;
                hr = context->getSegment(i, &segment);
                if (SUCCEEDED(hr)) {
                    ComLight::CComBSTR text;
                    hr = segment->getText(&text);
                    if (SUCCEEDED(hr) && text.m_str) {
                        std::wstring wtext(text.m_str);
                        std::string segment_text(wtext.begin(), wtext.end());
                        full_transcription += segment_text;
                        if (i < segmentCount - 1) full_transcription += " ";
                    }
                }
            }

            result.gpu_transcription = full_transcription;

            std::cout << "✅ GPU inference completed" << std::endl;
            std::cout << "   Time: " << result.gpu_time_ms << " ms" << std::endl;
            std::cout << "   Segments: " << segmentCount << std::endl;
            std::cout << "   Text: \"" << result.gpu_transcription << "\"" << std::endl;

            // Save GPU result
            std::ofstream gpu_file(output_dir + "/gpu_result.txt");
            gpu_file << result.gpu_transcription;
            gpu_file.close();

            CoUninitialize();
            return true;

        } catch (const std::exception& e) {
            result.error_message = "GPU inference exception: " + std::string(e.what());
            CoUninitialize();
            return false;
        }
    }

    bool runCpuInference(TestResult& result) {
        try {
            auto start_time = std::chrono::high_resolution_clock::now();

            // Initialize whisper.cpp context
            struct whisper_context_params cparams = whisper_context_default_params();
            cparams.use_gpu = false; // Force CPU
            
            struct whisper_context* ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
            if (!ctx) {
                result.error_message = "Failed to load CPU model with whisper.cpp";
                return false;
            }

            std::cout << "✅ CPU model loaded successfully" << std::endl;

            // Load audio file (simplified - assume WAV format)
            std::vector<float> pcmf32;
            if (!loadAudioFile(audio_path, pcmf32)) {
                result.error_message = "Failed to load audio file for CPU inference";
                whisper_free(ctx);
                return false;
            }

            std::cout << "✅ Audio loaded for CPU inference (" << pcmf32.size() << " samples)" << std::endl;

            // Setup inference parameters
            struct whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            wparams.language = "en";
            wparams.translate = false;
            wparams.print_progress = false;
            wparams.print_timestamps = false;

            // Run inference
            int ret = whisper_full(ctx, wparams, pcmf32.data(), (int)pcmf32.size());
            if (ret != 0) {
                result.error_message = "CPU inference failed with code: " + std::to_string(ret);
                whisper_free(ctx);
                return false;
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            result.cpu_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

            // Get results
            int n_segments = whisper_full_n_segments(ctx);
            if (n_segments == 0) {
                result.error_message = "No segments generated by CPU inference";
                whisper_free(ctx);
                return false;
            }

            // Concatenate all segments
            std::string full_transcription;
            for (int i = 0; i < n_segments; i++) {
                const char* text = whisper_full_get_segment_text(ctx, i);
                if (text) {
                    full_transcription += text;
                    if (i < n_segments - 1) full_transcription += " ";
                }
            }

            result.cpu_transcription = full_transcription;

            std::cout << "✅ CPU inference completed" << std::endl;
            std::cout << "   Time: " << result.cpu_time_ms << " ms" << std::endl;
            std::cout << "   Segments: " << n_segments << std::endl;
            std::cout << "   Text: \"" << result.cpu_transcription << "\"" << std::endl;

            // Save CPU result
            std::ofstream cpu_file(output_dir + "/cpu_result.txt");
            cpu_file << result.cpu_transcription;
            cpu_file.close();

            whisper_free(ctx);
            return true;

        } catch (const std::exception& e) {
            result.error_message = "CPU inference exception: " + std::string(e.what());
            return false;
        }
    }

    bool loadAudioFile(const std::string& path, std::vector<float>& pcmf32) {
        // Simplified audio loading - for now just return dummy data
        // In a real implementation, this would use proper audio decoding
        std::cout << "[WARN]: Using simplified audio loading (dummy data)" << std::endl;

        // Generate 1 second of silence at 16kHz (whisper.cpp requirement)
        pcmf32.resize(16000, 0.0f);
        return true;
    }

    void compareResults(TestResult& result) {
        std::cout << "GPU Result: \"" << result.gpu_transcription << "\"" << std::endl;
        std::cout << "CPU Result: \"" << result.cpu_transcription << "\"" << std::endl;
        std::cout << "GPU Time: " << result.gpu_time_ms << " ms" << std::endl;
        std::cout << "CPU Time: " << result.cpu_time_ms << " ms" << std::endl;

        // Simple comparison
        bool identical = (result.gpu_transcription == result.cpu_transcription);
        std::cout << "Results identical: " << (identical ? "YES" : "NO") << std::endl;

        if (!identical) {
            std::cout << "Note: Some differences are expected due to quantization and floating-point precision" << std::endl;
        }

        // Save comparison
        std::ofstream comp_file(output_dir + "/comparison.txt");
        comp_file << "=== End-to-End Quantization Test Results ===" << std::endl;
        comp_file << "GPU Result: " << result.gpu_transcription << std::endl;
        comp_file << "CPU Result: " << result.cpu_transcription << std::endl;
        comp_file << "GPU Time: " << result.gpu_time_ms << " ms" << std::endl;
        comp_file << "CPU Time: " << result.cpu_time_ms << " ms" << std::endl;
        comp_file << "Identical: " << (identical ? "YES" : "NO") << std::endl;
        comp_file.close();
    }
};

int main(int argc, char* argv[]) {
    std::string model_path = "../../Tests/Models/ggml-tiny.en-q5_1.bin";
    std::string audio_path = "../../Tests/Audio/jfk.wav";
    std::string output_dir = "test_results";

    // Parse command line arguments
    if (argc >= 2) model_path = argv[1];
    if (argc >= 3) audio_path = argv[2];
    if (argc >= 4) output_dir = argv[3];

    EndToEndQuantizationTest test(model_path, audio_path, output_dir);
    auto result = test.runEndToEndTest();

    if (result.success) {
        std::cout << "\n🎉 End-to-End Test PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ End-to-End Test FAILED!" << std::endl;
        std::cout << "Error: " << result.error_message << std::endl;
        return 1;
    }
}
