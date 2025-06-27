#include <iostream>
#include <string>
#include <filesystem>
#include "WhisperModel.h"

int main() {
    std::cout << "[INFO]: Quantization Spike - GGML Model Loading Test" << std::endl;
    std::cout << "=================================================" << std::endl;
    
    // Test model path
    std::string model_path = "F:/Projects/WhisperDesktopNG/TestModels/ggml-tiny.en-q5_1.bin";
    
    // Check if model exists
    if (!std::filesystem::exists(model_path)) {
        std::cerr << "[ERROR]: Test model not found: " << model_path << std::endl;
        std::cerr << "Please ensure the model file exists." << std::endl;
        return 1;
    }
    
    try {
        std::cout << "\n[INFO]: Step 1: Loading GGML model..." << std::endl;
        QuantizationSpike::WhisperCppModel model(model_path);

        std::cout << "\n[INFO]: Step 2: Validating model structure..." << std::endl;
        bool validation_result = model.validate_quantization();

        std::cout << "\n[INFO]: Step 3: Displaying model information..." << std::endl;
        model.print_model_info();
        
        std::cout << "\n[INFO]: Test Results:" << std::endl;
        std::cout << "  Model Loading: " << (model.is_loaded() ? "SUCCESS" : "FAILED") << std::endl;
        std::cout << "  Validation: " << (validation_result ? "PASSED" : "FAILED") << std::endl;
        
        if (model.is_loaded() && validation_result) {
            std::cout << "\n[PASS]: All tests PASSED! GGML quantized model loaded successfully." << std::endl;
            std::cout << "[PASS]: whisper.cpp integration is working correctly." << std::endl;
            std::cout << "[PASS]: Quantization support confirmed." << std::endl;
            return 0;
        } else {
            std::cout << "\n[FAIL]: Some tests FAILED!" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR]: Exception caught: " << e.what() << std::endl;
        std::cerr << "[FAIL]: Test FAILED due to exception." << std::endl;
        return 1;
    }
}
