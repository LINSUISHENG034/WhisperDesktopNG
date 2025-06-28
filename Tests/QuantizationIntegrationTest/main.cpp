#include <iostream>
#include <vector>
#include <memory>

// Test the integration of quantization support in the main project
// This test verifies that our quantization types are properly recognized

// Only include the basic enums header to avoid complex dependencies
#include "../../Whisper/D3D/enums.h"

using namespace DirectCompute;

int main()
{
    std::cout << "=== Quantization Integration Test ===" << std::endl;
    
    try {
        // Test 1: Verify quantization type enum values exist
        std::cout << "\n=== Testing Quantization Type Enum Values ===" << std::endl;

        eDataType q4_0_type = eDataType::Q4_0;
        eDataType q5_1_type = eDataType::Q5_1;
        eDataType q8_0_type = eDataType::Q8_0;
        eDataType fp32_type = eDataType::FP32;

        std::cout << "[INFO]: Q4_0 enum value: " << (int)q4_0_type << std::endl;
        std::cout << "[INFO]: Q5_1 enum value: " << (int)q5_1_type << std::endl;
        std::cout << "[INFO]: Q8_0 enum value: " << (int)q8_0_type << std::endl;
        std::cout << "[INFO]: FP32 enum value: " << (int)fp32_type << std::endl;

        // Verify that quantization types have different values
        bool enum_values_unique = (q4_0_type != q5_1_type) &&
                                (q5_1_type != q8_0_type) &&
                                (q8_0_type != fp32_type);

        if (enum_values_unique) {
            std::cout << "[PASS]: Quantization type enum values are unique" << std::endl;
        } else {
            std::cout << "[ERROR]: Quantization type enum values are not unique" << std::endl;
            return 1;
        }
        
        // Test 2: Verify elementSize function
        std::cout << "\n=== Testing elementSize Function ===" << std::endl;

        size_t q4_0_size = elementSize(eDataType::Q4_0);
        size_t q5_1_size = elementSize(eDataType::Q5_1);
        size_t q8_0_size = elementSize(eDataType::Q8_0);
        size_t fp32_size = elementSize(eDataType::FP32);

        std::cout << "[INFO]: Q4_0 element size: " << q4_0_size << " bytes" << std::endl;
        std::cout << "[INFO]: Q5_1 element size: " << q5_1_size << " bytes" << std::endl;
        std::cout << "[INFO]: Q8_0 element size: " << q8_0_size << " bytes" << std::endl;
        std::cout << "[INFO]: FP32 element size: " << fp32_size << " bytes" << std::endl;

        // Expected sizes based on GGML specification
        const size_t expected_q4_0 = 18;  // 2 + 16
        const size_t expected_q5_1 = 24;  // 2 + 2 + 4 + 16
        const size_t expected_q8_0 = 34;  // 2 + 32
        const size_t expected_fp32 = 4;   // 4 bytes per float

        bool sizes_correct = (q4_0_size == expected_q4_0) &&
                           (q5_1_size == expected_q5_1) &&
                           (q8_0_size == expected_q8_0) &&
                           (fp32_size == expected_fp32);

        if (sizes_correct) {
            std::cout << "[PASS]: elementSize function works correctly" << std::endl;
        } else {
            std::cout << "[ERROR]: elementSize function failed" << std::endl;
            std::cout << "  Expected: Q4_0=" << expected_q4_0 << ", Q5_1=" << expected_q5_1 << ", Q8_0=" << expected_q8_0 << ", FP32=" << expected_fp32 << std::endl;
            std::cout << "  Actual: Q4_0=" << q4_0_size << ", Q5_1=" << q5_1_size << ", Q8_0=" << q8_0_size << ", FP32=" << fp32_size << std::endl;
            return 1;
        }


        // Test summary
        std::cout << "\n=== Integration Test Summary ===" << std::endl;
        std::cout << "[PASS]: All quantization integration tests passed!" << std::endl;
        std::cout << "[PASS]: Quantization type enum values are unique" << std::endl;
        std::cout << "[PASS]: elementSize function works correctly" << std::endl;
        std::cout << "[PASS]: Ready for quantized model loading" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "[ERROR]: Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "[ERROR]: Unknown exception occurred" << std::endl;
        return 1;
    }
}
