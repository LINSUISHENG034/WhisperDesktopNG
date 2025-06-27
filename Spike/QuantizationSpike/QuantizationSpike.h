#pragma once

#include <string>
#include <vector>
#include <memory>
#include <windows.h>

// Forward declarations
class MinimalGGMLParser;
class QuantizedBufferManager;
class QuantizationDispatcher;

/**
 * @brief Main Spike class for end-to-end quantization validation
 * 
 * This class implements the core spike functionality to prove the technical
 * feasibility of loading quantized tensors from GGML files and dequantizing
 * them on the GPU using HLSL shaders.
 * 
 * Spike Goal: "Load a single quantized tensor and successfully dequantize it on GPU"
 */
class QuantizationSpike {
public:
    QuantizationSpike();
    ~QuantizationSpike();

    /**
     * @brief Initialize the spike components
     * @return S_OK on success, error HRESULT on failure
     */
    HRESULT initialize();

    /**
     * @brief Main spike entry point - proves quantization concept end-to-end
     * @param ggmlFile Path to the GGML file containing quantized tensors
     * @return S_OK on success, error HRESULT on failure
     */
    HRESULT proveQuantizationConcept(const std::string& ggmlFile);

    /**
     * @brief Cleanup resources
     */
    void cleanup();

private:
    // Core components
    std::unique_ptr<MinimalGGMLParser> m_parser;
    std::unique_ptr<QuantizedBufferManager> m_bufferMgr;
    std::unique_ptr<QuantizationDispatcher> m_dispatcher;

    // State tracking
    bool m_initialized;

    /**
     * @brief Verify GPU results against CPU reference implementation
     * @param tensorData Original quantized tensor data
     * @param gpuResult GPU dequantization result
     * @return S_OK if results match within epsilon, E_FAIL otherwise
     */
    HRESULT verifyAgainstCPUReference(const std::vector<uint8_t>& tensorData, 
                                    const std::vector<float>& gpuResult);

    /**
     * @brief Log detailed information about the spike execution
     * @param message Log message
     */
    void logInfo(const std::string& message);

    /**
     * @brief Log error information
     * @param message Error message
     * @param hr Error HRESULT
     */
    void logError(const std::string& message, HRESULT hr = E_FAIL);
};

// Helper structures for quantized tensor information
struct QuantizedTensorInfo {
    std::string name;
    std::vector<int> dimensions;
    int quantizationType;  // e.g., Q4_0, Q5_0, Q8_0
    size_t dataOffset;
    size_t dataSize;
    std::vector<uint8_t> data;
};

// Error codes specific to the spike
#define E_SPIKE_PARSER_FAILED           MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x301)
#define E_SPIKE_NO_QUANTIZED_TENSOR     MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x302)
#define E_SPIKE_GPU_BUFFER_FAILED       MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x303)
#define E_SPIKE_SHADER_DISPATCH_FAILED  MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x304)
#define E_SPIKE_VERIFICATION_FAILED     MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x305)
