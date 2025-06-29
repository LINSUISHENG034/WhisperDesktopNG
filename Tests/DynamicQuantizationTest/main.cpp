#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <iomanip>

// Include DirectCompute headers
#include "ML/QuantizationOps.h"
#include "ML/Tensor.h"
#include "D3D/enums.h"

using namespace DirectCompute;

class DynamicQuantizationTest {
public:
    struct TestResult {
        std::string quantization_type;
        bool success = false;
        double time_ms = 0.0;
        std::string error_message;
    };

    void runAllTests() {
        std::cout << "=== Dynamic Quantization Dispatch Test ===" << std::endl;
        
        std::vector<TestResult> results;
        
        // Test each quantization type
        results.push_back(testQuantizationType(eDataType::Q4_0, "Q4_0"));
        results.push_back(testQuantizationType(eDataType::Q5_1, "Q5_1"));
        results.push_back(testQuantizationType(eDataType::Q8_0, "Q8_0"));
        
        // Test legacy function
        results.push_back(testLegacyFunction());
        
        // Print results
        printResults(results);
    }

private:
    TestResult testQuantizationType(eDataType quantType, const std::string& typeName) {
        TestResult result;
        result.quantization_type = typeName;
        
        std::cout << "\n--- Testing " << typeName << " Dynamic Dispatch ---" << std::endl;
        
        try {
            // Create test tensors
            const uint32_t elementCount = 1024; // 32 blocks * 32 elements
            const uint32_t blockCount = elementCount / 32;
            
            // Create input tensor (quantized data)
            Tensor quantizedInput;
            uint32_t inputSize = blockCount * QuantizationOps::getBlockSize(quantType) / 4; // Convert to uint32 count
            HRESULT hr = quantizedInput.create(eDataType::U32, {inputSize});
            if (FAILED(hr)) {
                result.error_message = "Failed to create input tensor";
                return result;
            }
            
            // Create output tensor (FP32)
            Tensor fp32Output;
            hr = fp32Output.create(eDataType::FP32, {elementCount});
            if (FAILED(hr)) {
                result.error_message = "Failed to create output tensor";
                return result;
            }
            
            // Measure performance
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // Test the new dynamic dispatch function
            hr = QuantizationOps::dequantize(quantizedInput, fp32Output, quantType);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            result.time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            
            if (SUCCEEDED(hr)) {
                result.success = true;
                std::cout << "✅ " << typeName << " dispatch successful (" << result.time_ms << " ms)" << std::endl;
            } else {
                result.error_message = "Dequantization failed with HRESULT: 0x" + std::to_string(hr);
                std::cout << "❌ " << typeName << " dispatch failed" << std::endl;
            }
            
        } catch (const std::exception& e) {
            result.error_message = "Exception: " + std::string(e.what());
            std::cout << "❌ " << typeName << " exception: " << e.what() << std::endl;
        }
        
        return result;
    }
    
    TestResult testLegacyFunction() {
        TestResult result;
        result.quantization_type = "Legacy";
        
        std::cout << "\n--- Testing Legacy Function ---" << std::endl;
        
        try {
            // Create test tensors for Q5_1 (default fallback)
            const uint32_t elementCount = 1024;
            const uint32_t blockCount = elementCount / 32;
            
            // Create input tensor
            Tensor quantizedInput;
            uint32_t inputSize = blockCount * QuantizationOps::getBlockSize(eDataType::Q5_1) / 4;
            HRESULT hr = quantizedInput.create(eDataType::U32, {inputSize});
            if (FAILED(hr)) {
                result.error_message = "Failed to create input tensor";
                return result;
            }
            
            // Create output tensor
            Tensor fp32Output;
            hr = fp32Output.create(eDataType::FP32, {elementCount});
            if (FAILED(hr)) {
                result.error_message = "Failed to create output tensor";
                return result;
            }
            
            // Measure performance
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // Test the legacy function (without explicit type)
            hr = QuantizationOps::dequantize(quantizedInput, fp32Output);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            result.time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            
            if (SUCCEEDED(hr)) {
                result.success = true;
                std::cout << "✅ Legacy function successful (" << result.time_ms << " ms)" << std::endl;
            } else {
                result.error_message = "Legacy dequantization failed with HRESULT: 0x" + std::to_string(hr);
                std::cout << "❌ Legacy function failed" << std::endl;
            }
            
        } catch (const std::exception& e) {
            result.error_message = "Exception: " + std::string(e.what());
            std::cout << "❌ Legacy function exception: " << e.what() << std::endl;
        }
        
        return result;
    }
    
    void printResults(const std::vector<TestResult>& results) {
        std::cout << "\n=== Test Results Summary ===" << std::endl;
        std::cout << "| Type | Status | Time (ms) | Notes |" << std::endl;
        std::cout << "|------|--------|-----------|-------|" << std::endl;
        
        for (const auto& result : results) {
            std::string status = result.success ? "✅ PASS" : "❌ FAIL";
            std::string time_str = result.success ? std::to_string(result.time_ms) : "N/A";
            std::string notes = result.error_message.empty() ? "OK" : result.error_message;
            
            std::cout << "| " << std::setw(4) << result.quantization_type 
                      << " | " << std::setw(6) << status
                      << " | " << std::setw(9) << time_str
                      << " | " << notes << " |" << std::endl;
        }
        
        // Check if all tests passed
        bool all_passed = true;
        for (const auto& result : results) {
            if (!result.success) {
                all_passed = false;
                break;
            }
        }
        
        std::cout << "\n=== Final Result ===" << std::endl;
        if (all_passed) {
            std::cout << "🎉 All dynamic quantization dispatch tests PASSED!" << std::endl;
            std::cout << "✅ Task 1 (Dynamic Dequantization Scheduling) COMPLETE" << std::endl;
        } else {
            std::cout << "❌ Some tests FAILED. Please check the implementation." << std::endl;
        }
    }
};

int main() {
    try {
        DynamicQuantizationTest test;
        test.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cout << "❌ Test suite failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
