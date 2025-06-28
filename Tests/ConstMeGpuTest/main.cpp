#include <iostream>
#include <string>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <vector>
#include <windows.h>

// Include ComLight client library
#include "comLightClient.h"

// Include Const-me COM interfaces
#include "API/whisperComLight.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace fs = std::filesystem;

class ConstMeGpuTest {
private:
    std::string model_path;
    std::string audio_path;
    std::string output_dir;

public:
    ConstMeGpuTest(const std::string& model, const std::string& audio, const std::string& output)
        : model_path(model), audio_path(audio), output_dir(output) {}

    struct TestResult {
        bool success = false;
        std::string gpu_transcription;
        double gpu_time_ms = 0.0;
        std::string error_message;
        uint32_t segment_count = 0;
    };

    TestResult runGpuEndToEndTest() {
        TestResult result;
        
        std::cout << "=== Const-me GPU End-to-End Test ===" << std::endl;
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

        // Run GPU inference using Const-me COM interfaces
        if (!runConstMeGpuInference(result)) {
            return result;
        }

        result.success = true;
        return result;
    }

private:
    bool runConstMeGpuInference(TestResult& result) {
        try {
            std::cout << "--- Const-me GPU Inference ---" << std::endl;
            
            // Initialize COM
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(hr)) {
                result.error_message = "Failed to initialize COM, HRESULT: 0x" + std::to_string(hr);
                return false;
            }

            auto start_time = std::chrono::high_resolution_clock::now();

            // Step 1: Load model using Const-me GPU implementation
            std::cout << "[1/5] Loading model..." << std::endl;
            ComLight::CComPtr<Whisper::iModel> model;
            if (!loadModel(model, result)) {
                CoUninitialize();
                return false;
            }
            std::cout << "✅ Model loaded successfully" << std::endl;

            // Step 2: Create context
            std::cout << "[2/5] Creating context..." << std::endl;
            std::cout << "DEBUG: About to call model->createContext()" << std::endl;
            std::cout << "DEBUG: This is where whisper.cpp gets called unexpectedly!" << std::endl;

            ComLight::CComPtr<Whisper::iContext> context;
            hr = model->createContext(&context);

            std::cout << "DEBUG: createContext returned HRESULT = 0x" << std::hex << hr << std::dec << std::endl;

            if (FAILED(hr)) {
                result.error_message = "Failed to create context, HRESULT: 0x" + std::to_string(hr);
                CoUninitialize();
                return false;
            }
            std::cout << "✅ Context created successfully" << std::endl;

            // Step 3: Load audio file
            std::cout << "[3/5] Loading audio..." << std::endl;
            ComLight::CComPtr<Whisper::iAudioBuffer> audioBuffer;
            if (!loadAudio(audioBuffer, result)) {
                CoUninitialize();
                return false;
            }
            std::cout << "✅ Audio loaded successfully (" << audioBuffer->countSamples() << " samples)" << std::endl;

            // Step 4: Execute inference
            std::cout << "[4/5] Executing GPU inference..." << std::endl;
            if (!executeInference(context, audioBuffer, result)) {
                CoUninitialize();
                return false;
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            result.gpu_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

            // Step 5: Get results
            std::cout << "[5/5] Retrieving results..." << std::endl;
            if (!getResults(context, result)) {
                CoUninitialize();
                return false;
            }

            std::cout << "✅ GPU inference completed successfully" << std::endl;
            std::cout << "   Total time: " << result.gpu_time_ms << " ms" << std::endl;
            std::cout << "   Segments: " << result.segment_count << std::endl;
            std::cout << "   Transcription: \"" << result.gpu_transcription << "\"" << std::endl;

            // Save results
            saveResults(result);

            CoUninitialize();
            return true;

        } catch (const std::exception& e) {
            result.error_message = "GPU inference exception: " + std::string(e.what());
            CoUninitialize();
            return false;
        }
    }

    bool loadModel(ComLight::CComPtr<Whisper::iModel>& model, TestResult& result) {
        // First, try to list available GPUs
        std::cout << "Checking available GPUs..." << std::endl;

        // Setup model configuration for GPU with quantization support
        Whisper::sModelSetup setup = {};
        setup.impl = Whisper::eModelImplementation::GPU;
        setup.flags = 0; // Default flags

        // Convert path to wide string
        std::wstring wmodel_path(model_path.begin(), model_path.end());

        std::cout << "Attempting to load model with GPU implementation..." << std::endl;
        std::cout << "DEBUG: setup.impl = " << (int)setup.impl << " (1=GPU, 2=Hybrid, 3=Reference)" << std::endl;
        std::cout << "DEBUG: setup.flags = " << setup.flags << std::endl;

        HRESULT hr = Whisper::loadModel(wmodel_path.c_str(), setup, nullptr, &model);
        std::cout << "DEBUG: loadModel returned HRESULT = 0x" << std::hex << hr << std::dec << std::endl;

        if (FAILED(hr)) {
            std::cout << "GPU model loading failed, trying Reference implementation..." << std::endl;

            // Try Reference implementation as fallback
            setup.impl = Whisper::eModelImplementation::Reference;
            hr = Whisper::loadModel(wmodel_path.c_str(), setup, nullptr, &model);
            if (FAILED(hr)) {
                result.error_message = "Failed to load model with both GPU and Reference implementations, HRESULT: 0x" + std::to_string(hr);
                return false;
            }
            std::cout << "Successfully loaded model with Reference implementation" << std::endl;
        } else {
            std::cout << "Successfully loaded model with GPU implementation" << std::endl;
        }

        return true;
    }

    bool loadAudio(ComLight::CComPtr<Whisper::iAudioBuffer>& audioBuffer, TestResult& result) {
        // Initialize Media Foundation
        ComLight::CComPtr<Whisper::iMediaFoundation> mf;
        HRESULT hr = Whisper::initMediaFoundation(&mf);
        if (FAILED(hr)) {
            result.error_message = "Failed to initialize Media Foundation, HRESULT: 0x" + std::to_string(hr);
            return false;
        }

        // Convert path to wide string
        std::wstring waudio_path(audio_path.begin(), audio_path.end());
        
        // Load audio file (mono)
        hr = mf->loadAudioFile(waudio_path.c_str(), false, &audioBuffer);
        if (FAILED(hr)) {
            result.error_message = "Failed to load audio file, HRESULT: 0x" + std::to_string(hr);
            return false;
        }

        return true;
    }

    bool executeInference(ComLight::CComPtr<Whisper::iContext>& context, 
                         ComLight::CComPtr<Whisper::iAudioBuffer>& audioBuffer, 
                         TestResult& result) {
        // Setup inference parameters
        Whisper::sFullParams params = {};
        
        // Get default parameters
        HRESULT hr = context->fullDefaultParams(Whisper::eSamplingStrategy::Greedy, &params);
        if (FAILED(hr)) {
            result.error_message = "Failed to get default parameters, HRESULT: 0x" + std::to_string(hr);
            return false;
        }

        // Configure for English transcription
        params.language = Whisper::makeLanguageKey("en");
        params.flags = Whisper::eFullParamsFlags::PrintProgress;

        // Execute inference
        hr = context->runFull(params, audioBuffer);
        if (FAILED(hr)) {
            result.error_message = "GPU inference failed, HRESULT: 0x" + std::to_string(hr);
            return false;
        }

        return true;
    }

    bool getResults(ComLight::CComPtr<Whisper::iContext>& context, TestResult& result) {
        // Get transcription results
        ComLight::CComPtr<Whisper::iTranscribeResult> transcribeResult;
        HRESULT hr = context->getResults(Whisper::eResultFlags::None, &transcribeResult);
        if (FAILED(hr)) {
            result.error_message = "Failed to get results, HRESULT: 0x" + std::to_string(hr);
            return false;
        }

        // Get segment information
        Whisper::sTranscribeLength length;
        hr = transcribeResult->getSize(length);
        if (FAILED(hr)) {
            result.error_message = "Failed to get result size, HRESULT: 0x" + std::to_string(hr);
            return false;
        }

        result.segment_count = length.countSegments;

        // Get segments array
        const Whisper::sSegment* segments = transcribeResult->getSegments();
        if (!segments) {
            result.error_message = "Failed to get segments array";
            return false;
        }

        // Concatenate all segments
        std::string full_transcription;
        for (uint32_t i = 0; i < result.segment_count; i++) {
            if (segments[i].text) {
                full_transcription += segments[i].text;
                if (i < result.segment_count - 1) full_transcription += " ";
            }
        }

        result.gpu_transcription = full_transcription;
        return true;
    }

    void saveResults(const TestResult& result) {
        // Save GPU result
        std::ofstream gpu_file(output_dir + "/gpu_result.txt");
        gpu_file << "=== Const-me GPU Inference Results ===" << std::endl;
        gpu_file << "Model: " << model_path << std::endl;
        gpu_file << "Audio: " << audio_path << std::endl;
        gpu_file << "Time: " << result.gpu_time_ms << " ms" << std::endl;
        gpu_file << "Segments: " << result.segment_count << std::endl;
        gpu_file << "Transcription: " << result.gpu_transcription << std::endl;
        gpu_file.close();

        std::cout << "✅ Results saved to " << output_dir << "/gpu_result.txt" << std::endl;
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

    std::cout << "Const-me GPU End-to-End Quantization Test" << std::endl;
    std::cout << "=========================================" << std::endl << std::endl;

    ConstMeGpuTest test(model_path, audio_path, output_dir);
    auto result = test.runGpuEndToEndTest();

    if (result.success) {
        std::cout << "\n🎉 Const-me GPU Test PASSED!" << std::endl;
        std::cout << "GPU Transcription: \"" << result.gpu_transcription << "\"" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Const-me GPU Test FAILED!" << std::endl;
        std::cout << "Error: " << result.error_message << std::endl;
        return 1;
    }
}
