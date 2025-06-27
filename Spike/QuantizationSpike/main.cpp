#include <iostream>
#include <string>
#include <windows.h>
#include "QuantizationSpike.h"

/**
 * @brief Main entry point for the Quantization Spike
 * 
 * This spike validates the core technical assumption:
 * "Load a single quantized tensor from GGML file and successfully dequantize it on GPU"
 */
int main(int argc, char* argv[]) {
    std::cout << "=== Quantization Spike - Technical Feasibility Validation ===" << std::endl;
    std::cout << "Goal: Load GGML quantized tensor and dequantize on GPU" << std::endl;
    std::cout << "=============================================================" << std::endl;

    // Parse command line arguments
    std::string ggmlFile;
    if (argc > 1) {
        ggmlFile = argv[1];
    } else {
        // Default test file
        ggmlFile = "../../TestModels/ggml-tiny.en-q5_1.bin";
    }

    std::cout << "Using GGML file: " << ggmlFile << std::endl;

    // Initialize COM for D3D11
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM: 0x" << std::hex << hr << std::endl;
        return -1;
    }

    // Create and run the spike
    QuantizationSpike spike;
    
    hr = spike.initialize();
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize spike: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return -1;
    }

    std::cout << "\n--- Starting Spike Execution ---" << std::endl;
    hr = spike.proveQuantizationConcept(ggmlFile);
    
    if (SUCCEEDED(hr)) {
        std::cout << "\n🎉 SPIKE SUCCESS! Technical feasibility validated!" << std::endl;
        std::cout << "✅ Core assumption proven: GGML quantized tensors can be processed on GPU" << std::endl;
        std::cout << "✅ Ready to proceed with full implementation" << std::endl;
    } else {
        std::cout << "\n❌ SPIKE FAILED! Technical approach needs adjustment" << std::endl;
        std::cout << "❌ Error code: 0x" << std::hex << hr << std::endl;
        std::cout << "❌ Review technical assumptions and implementation" << std::endl;
    }

    // Cleanup
    spike.cleanup();
    CoUninitialize();

    std::cout << "\n--- Spike Execution Complete ---" << std::endl;
    
    // Wait for user input in debug mode
#ifdef _DEBUG
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
#endif

    return SUCCEEDED(hr) ? 0 : -1;
}
