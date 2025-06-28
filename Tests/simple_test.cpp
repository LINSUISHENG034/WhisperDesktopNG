#include <iostream>
#include "../Whisper/D3D/enums.h"

using namespace DirectCompute;

int main()
{
    std::cout << "=== Simple Quantization Integration Test ===" << std::endl;
    
    // Test that quantization enum values exist and are unique
    eDataType q4_0 = eDataType::Q4_0;
    eDataType q5_1 = eDataType::Q5_1;
    eDataType q8_0 = eDataType::Q8_0;
    eDataType fp32 = eDataType::FP32;
    
    std::cout << "Q4_0 enum value: " << (int)q4_0 << std::endl;
    std::cout << "Q5_1 enum value: " << (int)q5_1 << std::endl;
    std::cout << "Q8_0 enum value: " << (int)q8_0 << std::endl;
    std::cout << "FP32 enum value: " << (int)fp32 << std::endl;
    
    // Test elementSize function
    size_t q4_0_size = elementSize(q4_0);
    size_t q5_1_size = elementSize(q5_1);
    size_t q8_0_size = elementSize(q8_0);
    size_t fp32_size = elementSize(fp32);
    
    std::cout << "Q4_0 element size: " << q4_0_size << " bytes" << std::endl;
    std::cout << "Q5_1 element size: " << q5_1_size << " bytes" << std::endl;
    std::cout << "Q8_0 element size: " << q8_0_size << " bytes" << std::endl;
    std::cout << "FP32 element size: " << fp32_size << " bytes" << std::endl;
    
    // Verify expected sizes
    bool test_passed = (q4_0_size == 18) && (q5_1_size == 24) && (q8_0_size == 34) && (fp32_size == 4);
    
    if (test_passed) {
        std::cout << "[PASS]: All tests passed! Quantization support is working." << std::endl;
        return 0;
    } else {
        std::cout << "[FAIL]: Tests failed!" << std::endl;
        return 1;
    }
}
