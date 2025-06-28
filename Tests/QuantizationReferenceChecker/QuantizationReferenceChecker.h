#pragma once

#include <string>
#include <vector>
#include <memory>
#include <windows.h>
#include <unknwn.h>

// Forward declarations to avoid include issues for now
struct ggml_tensor;
struct whisper_context;
struct whisper_model;
struct whisper_layer_encoder;
enum ggml_type : int;

/**
 * @brief CPU Reference Implementation for Quantization Validation
 * 
 * This class provides the "golden standard" CPU implementation for quantized
 * tensor dequantization using whisper.cpp/GGML functions. It serves as the
 * reference for validating GPU dequantization implementations.
 * 
 * Key Features:
 * - Uses official GGML CPU dequantization functions
 * - Supports Q4_0, Q5_1, Q8_0 quantization formats
 * - Provides epsilon-based comparison for GPU validation
 * - Loads quantized tensors from GGML model files
 */
class QuantizationReferenceChecker {
public:
    QuantizationReferenceChecker();
    ~QuantizationReferenceChecker();

    /**
     * @brief Initialize the reference checker
     * @return S_OK on success, error HRESULT on failure
     */
    HRESULT initialize();

    /**
     * @brief Load a quantized model and prepare for tensor access
     * @param ggmlFilePath Path to the GGML model file
     * @return S_OK on success, error HRESULT on failure
     */
    HRESULT loadModel(const std::string& ggmlFilePath);

    /**
     * @brief Get CPU reference dequantization result for a tensor
     * @param tensorName Name of the tensor to dequantize
     * @param cpuResult Output vector containing dequantized float values
     * @return S_OK on success, error HRESULT on failure
     */
    HRESULT getCPUReference(const std::string& tensorName, std::vector<float>& cpuResult);

    /**
     * @brief Verify GPU result against CPU reference
     * @param gpuResult GPU dequantization result
     * @param cpuReference CPU reference result
     * @param epsilon Tolerance for floating-point comparison (default: 1e-6f)
     * @return true if results match within epsilon, false otherwise
     */
    bool verifyGPUResult(const std::vector<float>& gpuResult,
                        const std::vector<float>& cpuReference,
                        float epsilon = 1e-6f);

    // Forward declare TensorInfo
    struct TensorInfo;

    /**
     * @brief Get detailed information about a quantized tensor
     * @param tensorName Name of the tensor
     * @param info Output structure containing tensor information
     * @return S_OK on success, error HRESULT on failure
     */
    HRESULT getTensorInfo(const std::string& tensorName, TensorInfo& info);

    /**
     * @brief Run comprehensive validation suite on a model
     * @param ggmlFilePath Path to the GGML model file
     * @return S_OK if all validations pass, error HRESULT on failure
     */
    HRESULT runValidationSuite(const std::string& ggmlFilePath);

    /**
     * @brief Get raw quantized data for a tensor (for GPU upload)
     * @param tensorName Name of the tensor
     * @param quantizedData Output vector containing raw quantized bytes
     * @return S_OK on success, error HRESULT on failure
     */
    HRESULT getQuantizedData(const std::string& tensorName, std::vector<uint8_t>& quantizedData);

public:
    // Tensor information structure
    struct TensorInfo {
        std::string name;
        ggml_type type;
        std::vector<int64_t> dimensions;
        size_t totalElements;
        size_t quantizedSizeBytes;
        size_t dequantizedSizeBytes;
        bool isQuantized;
    };

private:
    // Internal implementation
    struct whisper_context* m_whisperContext;
    bool m_initialized;
    std::string m_currentModelPath;

    /**
     * @brief Find a tensor by name in the loaded model
     * @param tensorName Name of the tensor to find
     * @return Pointer to ggml_tensor or nullptr if not found
     */
    const struct ggml_tensor* findTensor(const std::string& tensorName);

    /**
     * @brief Dequantize a tensor using appropriate GGML function
     * @param tensor Pointer to the quantized tensor
     * @param output Output vector for dequantized values
     * @return S_OK on success, error HRESULT on failure
     */
    HRESULT dequantizeTensor(const struct ggml_tensor* tensor, std::vector<float>& output);

    /**
     * @brief Log detailed comparison results
     * @param gpuResult GPU result
     * @param cpuReference CPU reference
     * @param epsilon Comparison epsilon
     * @param tensorName Name of the tensor being compared
     */
    void logComparisonDetails(const std::vector<float>& gpuResult,
                             const std::vector<float>& cpuReference,
                             float epsilon,
                             const std::string& tensorName);

    /**
     * @brief Cleanup resources
     */
    void cleanup();
};

/**
 * @brief Utility functions for quantization validation
 */
namespace QuantizationUtils {
    /**
     * @brief Get human-readable name for GGML type
     * @param type GGML tensor type
     * @return String representation of the type
     */
    std::string getTypeName(ggml_type type);

    /**
     * @brief Check if a type is quantized
     * @param type GGML tensor type
     * @return true if quantized, false otherwise
     */
    bool isQuantizedType(ggml_type type);

    /**
     * @brief Get block size for quantized type
     * @param type GGML tensor type
     * @return Block size in elements
     */
    size_t getBlockSize(ggml_type type);

    /**
     * @brief Calculate expected dequantized size
     * @param quantizedSize Size of quantized data in bytes
     * @param type GGML tensor type
     * @return Expected size after dequantization in bytes
     */
    size_t calculateDequantizedSize(size_t quantizedSize, ggml_type type);
}
