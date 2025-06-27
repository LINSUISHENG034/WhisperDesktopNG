#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <fstream>

// GGML headers - wrapped in extern "C" for C++ compatibility
extern "C" {
    #include "ggml.h"
    #include "ggml-alloc.h"
    #include "ggml-backend.h"
    #include "ggml-cpu.h"
}

// Whisper headers - has its own extern "C" protection
#include "whisper.h"

struct ModelTestResult {
    std::string model_path;
    std::string quantization_type;
    bool load_success;
    size_t model_size_mb;
    double load_time_ms;
    std::string error_message;
};

class QuantizedModelTester {
private:
    std::vector<std::string> test_models;
    
public:
    QuantizedModelTester() {
        // Add test models (these should be downloaded separately)
        test_models = {
            "models/ggml-tiny.en-q4_0.bin",
            "models/ggml-tiny.en-q4_1.bin", 
            "models/ggml-tiny.en-q5_0.bin",
            "models/ggml-tiny.en-q5_1.bin",
            "models/ggml-tiny.en-q8_0.bin",
            "models/ggml-base.en-q4_0.bin",
            "models/ggml-base.en-q5_1.bin"
        };
    }
    
    std::string extractQuantizationType(const std::string& path) {
        size_t pos = path.find("-q");
        if (pos != std::string::npos) {
            size_t end = path.find(".bin", pos);
            if (end != std::string::npos) {
                return path.substr(pos + 1, end - pos - 1);
            }
        }
        return "unknown";
    }
    
    ModelTestResult testModel(const std::string& model_path) {
        ModelTestResult result;
        result.model_path = model_path;
        result.quantization_type = extractQuantizationType(model_path);
        result.load_success = false;
        result.model_size_mb = 0;
        result.load_time_ms = 0;
        
        // Check if file exists and get size
        std::ifstream file(model_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            result.error_message = "File not found or cannot open";
            return result;
        }

        // Get file size
        auto file_size = file.tellg();
        file.close();

        if (file_size > 0) {
            result.model_size_mb = static_cast<size_t>(file_size) / (1024 * 1024);
        } else {
            result.error_message = "Cannot get file size";
            return result;
        }
        
        // Test model loading
        auto start_time = std::chrono::high_resolution_clock::now();
        
        struct whisper_context_params cparams = whisper_context_default_params();
        cparams.use_gpu = false; // Use CPU for testing
        
        struct whisper_context* ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.load_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        if (ctx != nullptr) {
            result.load_success = true;
            
            // Get basic model information
            int n_vocab = whisper_n_vocab(ctx);
            int n_audio_ctx = whisper_n_audio_ctx(ctx);
            int n_text_ctx = whisper_n_text_ctx(ctx);

            std::cout << "    Model Info:" << std::endl;
            std::cout << "      Vocabulary size: " << n_vocab << std::endl;
            std::cout << "      Audio context: " << n_audio_ctx << std::endl;
            std::cout << "      Text context: " << n_text_ctx << std::endl;
            
            whisper_free(ctx);
        } else {
            result.error_message = "Failed to load model";
        }
        
        return result;
    }
    
    void runAllTests() {
        std::cout << "=== Quantized Model Compatibility Test ===" << std::endl;
        std::cout << "Testing GGML integration with various quantization types..." << std::endl << std::endl;
        
        std::vector<ModelTestResult> results;
        
        for (const auto& model_path : test_models) {
            std::cout << "Testing: " << model_path << std::endl;
            
            ModelTestResult result = testModel(model_path);
            results.push_back(result);
            
            if (result.load_success) {
                std::cout << "  ✅ SUCCESS" << std::endl;
                std::cout << "    Quantization: " << result.quantization_type << std::endl;
                std::cout << "    Size: " << result.model_size_mb << " MB" << std::endl;
                std::cout << "    Load time: " << std::fixed << std::setprecision(2) << result.load_time_ms << " ms" << std::endl;
            } else {
                std::cout << "  ❌ FAILED: " << result.error_message << std::endl;
            }
            std::cout << std::endl;
        }
        
        // Summary
        std::cout << "=== Test Summary ===" << std::endl;
        int successful = 0;
        int total = results.size();
        
        for (const auto& result : results) {
            if (result.load_success) {
                successful++;
            }
        }
        
        std::cout << "Successful tests: " << successful << "/" << total << std::endl;
        std::cout << "Success rate: " << std::fixed << std::setprecision(1) << (100.0 * successful / total) << "%" << std::endl;
        
        if (successful == total) {
            std::cout << "🎉 All quantization types are supported!" << std::endl;
        } else {
            std::cout << "⚠️  Some quantization types failed. Check model availability." << std::endl;
        }
    }
};

int main() {
    std::cout << "GGML Quantized Model Compatibility Tester" << std::endl;
    std::cout << "=========================================" << std::endl << std::endl;

    // Simple test - just try to load one model
    std::string test_model = "TestModels/ggml-tiny.en-q5_1.bin";

    std::cout << "Testing basic model loading..." << std::endl;

    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;

    struct whisper_context* ctx = whisper_init_from_file_with_params(test_model.c_str(), cparams);

    if (ctx != nullptr) {
        std::cout << "✅ Model loaded successfully!" << std::endl;

        int n_vocab = whisper_n_vocab(ctx);
        std::cout << "Vocabulary size: " << n_vocab << std::endl;

        whisper_free(ctx);
        std::cout << "✅ Model freed successfully!" << std::endl;
    } else {
        std::cout << "❌ Failed to load model: " << test_model << std::endl;
        std::cout << "Note: Make sure the model file exists in the models/ directory" << std::endl;
    }

    return 0;
}
