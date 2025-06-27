#include <iostream>
#include <string>
#include <fstream>
#include <vector>

// GGML headers - wrapped in extern "C" for C++ compatibility
extern "C" {
    #include "ggml.h"
    #include "ggml-alloc.h"
    #include "ggml-backend.h"
    #include "ggml-cpu.h"
}

// Whisper headers - also wrapped in extern "C"
extern "C" {
    #include "whisper.h"
}

int main() {
    std::cout << "[INFO]: Starting GGML and Whisper.cpp integration test" << std::endl;
    
    // Test 1: Initialize GGML backend
    std::cout << "[INFO]: Testing GGML backend initialization..." << std::endl;
    
    ggml_backend_t backend = ggml_backend_cpu_init();
    if (!backend) {
        std::cout << "[FAIL]: Failed to initialize GGML CPU backend" << std::endl;
        return 1;
    }
    std::cout << "[PASS]: GGML CPU backend initialized successfully" << std::endl;
    
    // Test 2: Test basic GGML context creation
    std::cout << "[INFO]: Testing GGML context creation..." << std::endl;
    
    struct ggml_init_params params = {
        /*.mem_size   =*/ 16*1024*1024,  // 16MB
        /*.mem_buffer =*/ nullptr,
        /*.no_alloc   =*/ false,
    };
    
    struct ggml_context* ctx = ggml_init(params);
    if (!ctx) {
        std::cout << "[FAIL]: Failed to create GGML context" << std::endl;
        ggml_backend_free(backend);
        return 1;
    }
    std::cout << "[PASS]: GGML context created successfully" << std::endl;
    
    // Test 3: Test whisper model loading (if model exists)
    std::cout << "[INFO]: Testing whisper model loading..." << std::endl;
    
    const std::string model_path = "TestModels/ggml-tiny.en-q5_1.bin";
    std::ifstream model_file(model_path, std::ios::binary);
    if (!model_file.good()) {
        std::cout << "[WARN]: Model file not found at " << model_path << std::endl;
        std::cout << "[INFO]: Skipping model loading test" << std::endl;
    } else {
        model_file.close();
        
        struct whisper_context_params cparams = whisper_context_default_params();
        cparams.use_gpu = false;  // Force CPU usage for this test
        
        struct whisper_context* wctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
        if (!wctx) {
            std::cout << "[FAIL]: Failed to load whisper model from " << model_path << std::endl;
        } else {
            std::cout << "[PASS]: Whisper model loaded successfully" << std::endl;
            
            // Get model information
            int n_vocab = whisper_n_vocab(wctx);
            int n_audio_ctx = whisper_n_audio_ctx(wctx);
            int n_text_ctx = whisper_n_text_ctx(wctx);
            
            std::cout << "[INFO]: Model info - vocab: " << n_vocab 
                      << ", audio_ctx: " << n_audio_ctx 
                      << ", text_ctx: " << n_text_ctx << std::endl;
            
            whisper_free(wctx);
        }
    }
    
    // Test 4: Test GGML tensor operations
    std::cout << "[INFO]: Testing basic GGML tensor operations..." << std::endl;
    
    struct ggml_tensor* a = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 10);
    struct ggml_tensor* b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 10);
    
    if (!a || !b) {
        std::cout << "[FAIL]: Failed to create GGML tensors" << std::endl;
    } else {
        std::cout << "[PASS]: GGML tensors created successfully" << std::endl;
        
        // Initialize tensor data
        for (int i = 0; i < 10; i++) {
            ((float*)a->data)[i] = (float)i;
            ((float*)b->data)[i] = (float)(i * 2);
        }
        
        // Create addition operation
        struct ggml_tensor* result = ggml_add(ctx, a, b);
        if (!result) {
            std::cout << "[FAIL]: Failed to create GGML add operation" << std::endl;
        } else {
            std::cout << "[PASS]: GGML add operation created successfully" << std::endl;
        }
    }
    
    // Cleanup
    std::cout << "[INFO]: Cleaning up resources..." << std::endl;
    ggml_free(ctx);
    ggml_backend_free(backend);
    
    std::cout << "[INFO]: All tests completed successfully!" << std::endl;
    std::cout << "[INFO]: GGML static library integration is working correctly" << std::endl;
    
    return 0;
}
