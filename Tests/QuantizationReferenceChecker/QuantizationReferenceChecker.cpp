#include "QuantizationReferenceChecker.h"

// GGML includes
#include "ggml.h"
#include "ggml-quants.h"
#include "ggml-common.h"
#include "whisper.h"

#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <iomanip>

QuantizationReferenceChecker::QuantizationReferenceChecker()
    : m_whisperContext(nullptr)
    , m_initialized(false)
{
}

QuantizationReferenceChecker::~QuantizationReferenceChecker() {
    cleanup();
}

HRESULT QuantizationReferenceChecker::initialize() {
    if (m_initialized) {
        return S_OK;
    }

    std::cout << "[INFO]: Initializing QuantizationReferenceChecker..." << std::endl;
    
    // Initialize GGML time (required for some operations)
    ggml_time_init();
    
    m_initialized = true;
    std::cout << "[INFO]: QuantizationReferenceChecker initialized successfully" << std::endl;
    
    return S_OK;
}

HRESULT QuantizationReferenceChecker::loadModel(const std::string& ggmlFilePath) {
    if (!m_initialized) {
        std::cout << "[ERROR]: Reference checker not initialized" << std::endl;
        return E_FAIL;
    }

    // Cleanup previous model if any
    if (m_whisperContext) {
        whisper_free(m_whisperContext);
        m_whisperContext = nullptr;
    }

    std::cout << "[INFO]: Loading model: " << ggmlFilePath << std::endl;

    // Check if file exists
    std::ifstream file(ggmlFilePath, std::ios::binary);
    if (!file.good()) {
        std::cout << "[ERROR]: Model file not found: " << ggmlFilePath << std::endl;
        return E_FAIL;
    }
    file.close();

    // Load model using whisper.cpp directly
    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;  // Force CPU for reference implementation

    m_whisperContext = whisper_init_from_file_with_params(ggmlFilePath.c_str(), cparams);
    if (!m_whisperContext) {
        std::cout << "[ERROR]: Failed to load model with whisper.cpp" << std::endl;
        return E_FAIL;
    }

    m_currentModelPath = ggmlFilePath;
    std::cout << "[INFO]: Model loaded successfully" << std::endl;

    return S_OK;
}

HRESULT QuantizationReferenceChecker::getCPUReference(const std::string& tensorName, std::vector<float>& cpuResult) {
    if (!m_whisperContext) {
        std::cout << "[ERROR]: No model loaded" << std::endl;
        return E_FAIL;
    }

    const struct ggml_tensor* tensor = findTensor(tensorName);
    if (!tensor) {
        std::cout << "[ERROR]: Tensor not found: " << tensorName << std::endl;
        return E_FAIL;
    }

    return dequantizeTensor(tensor, cpuResult);
}

bool QuantizationReferenceChecker::verifyGPUResult(const std::vector<float>& gpuResult,
                                                   const std::vector<float>& cpuReference,
                                                   float epsilon) {
    if (gpuResult.size() != cpuReference.size()) {
        std::cout << "[ERROR]: Size mismatch - GPU: " << gpuResult.size() 
                  << ", CPU: " << cpuReference.size() << std::endl;
        return false;
    }

    size_t mismatchCount = 0;
    float maxDifference = 0.0f;
    size_t maxDiffIndex = 0;

    for (size_t i = 0; i < gpuResult.size(); ++i) {
        float diff = std::abs(gpuResult[i] - cpuReference[i]);
        if (diff > epsilon) {
            mismatchCount++;
            if (diff > maxDifference) {
                maxDifference = diff;
                maxDiffIndex = i;
            }
        }
    }

    if (mismatchCount > 0) {
        std::cout << "[ERROR]: Validation failed!" << std::endl;
        std::cout << "  Mismatches: " << mismatchCount << " / " << gpuResult.size() << std::endl;
        std::cout << "  Max difference: " << maxDifference << " at index " << maxDiffIndex << std::endl;
        std::cout << "  GPU[" << maxDiffIndex << "] = " << gpuResult[maxDiffIndex] << std::endl;
        std::cout << "  CPU[" << maxDiffIndex << "] = " << cpuReference[maxDiffIndex] << std::endl;
        std::cout << "  Epsilon: " << epsilon << std::endl;
        return false;
    }

    std::cout << "[PASS]: GPU result matches CPU reference within epsilon " << epsilon << std::endl;
    return true;
}

HRESULT QuantizationReferenceChecker::getTensorInfo(const std::string& tensorName, TensorInfo& info) {
    if (!m_whisperContext) {
        return E_FAIL;
    }

    const struct ggml_tensor* tensor = this->findTensor(tensorName);
    if (!tensor) {
        return E_FAIL;
    }

    info.name = tensorName;
    info.type = tensor->type;
    info.dimensions.clear();

    int n_dims = ggml_n_dims(tensor);
    for (int i = 0; i < n_dims; ++i) {
        info.dimensions.push_back(tensor->ne[i]);
    }

    info.totalElements = ggml_nelements(tensor);
    info.quantizedSizeBytes = ggml_nbytes(tensor);
    info.dequantizedSizeBytes = info.totalElements * sizeof(float);
    info.isQuantized = QuantizationUtils::isQuantizedType(tensor->type);

    return S_OK;
}

HRESULT QuantizationReferenceChecker::getQuantizedData(const std::string& tensorName, std::vector<uint8_t>& quantizedData) {
    if (!m_whisperContext) {
        return E_FAIL;
    }

    const struct ggml_tensor* tensor = findTensor(tensorName);
    if (!tensor) {
        return E_FAIL;
    }

    size_t dataSize = ggml_nbytes(tensor);
    quantizedData.resize(dataSize);
    
    // Copy raw quantized data
    memcpy(quantizedData.data(), tensor->data, dataSize);

    return S_OK;
}

const struct ggml_tensor* QuantizationReferenceChecker::findTensor(const std::string& tensorName) {
    if (!m_whisperContext) {
        return nullptr;
    }

    std::cout << "[INFO]: Searching for tensor: " << tensorName << std::endl;

    // Use the new whisper_get_tensor_by_name API (expert-recommended solution)
    struct ggml_tensor* tensor = whisper_get_tensor_by_name(m_whisperContext, tensorName.c_str());
    if (tensor) {
        std::cout << "[INFO]: Found tensor: " << tensorName << std::endl;
        return tensor;
    }

    // If not found, try some common name variations
    std::vector<std::string> nameVariations = {
        "encoder." + tensorName,
        "decoder." + tensorName
    };

    for (const auto& name : nameVariations) {
        tensor = whisper_get_tensor_by_name(m_whisperContext, name.c_str());
        if (tensor) {
            std::cout << "[INFO]: Found tensor with variation: " << name << std::endl;
            return tensor;
        }
    }

    std::cout << "[WARN]: Tensor not found: " << tensorName << std::endl;
    std::cout << "[INFO]: Tried variations: encoder." << tensorName << ", decoder." << tensorName << std::endl;

    // If tensor not found and this is a debug request, list some available tensors
    if (tensorName == "list_tensors" || tensorName == "debug") {
        std::cout << "[INFO]: Listing first 20 available tensors:" << std::endl;
        int count = 0;
        for (int i = 0; i < 100 && count < 20; ++i) {
            std::string testName = "encoder.blocks." + std::to_string(i) + ".attn.query.weight";
            struct ggml_tensor* testTensor = whisper_get_tensor_by_name(m_whisperContext, testName.c_str());
            if (testTensor) {
                std::cout << "[INFO]:   " << testName << " (type: " << QuantizationUtils::getTypeName(testTensor->type) << ")" << std::endl;
                count++;
            }
        }
        // Try some common patterns
        std::vector<std::string> commonPatterns = {
            "encoder.blocks.0.attn.query.weight",
            "encoder.blocks.0.attn.key.weight",
            "encoder.blocks.0.attn.value.weight",
            "encoder.blocks.0.mlp.0.weight",
            "encoder.blocks.0.mlp.2.weight",
            "decoder.blocks.0.attn.query.weight",
            "decoder.token_embedding.weight",
            "decoder.positional_embedding"
        };
        for (const auto& pattern : commonPatterns) {
            struct ggml_tensor* testTensor = whisper_get_tensor_by_name(m_whisperContext, pattern.c_str());
            if (testTensor && count < 20) {
                std::cout << "[INFO]:   " << pattern << " (type: " << QuantizationUtils::getTypeName(testTensor->type) << ")" << std::endl;
                count++;
            }
        }
    }

    return nullptr;
}

HRESULT QuantizationReferenceChecker::dequantizeTensor(const struct ggml_tensor* tensor, std::vector<float>& output) {
    if (!tensor) {
        return E_POINTER;
    }

    size_t totalElements = ggml_nelements(tensor);
    output.resize(totalElements);

    std::cout << "[INFO]: Dequantizing tensor type: " << QuantizationUtils::getTypeName(tensor->type) << std::endl;
    std::cout << "[INFO]: Elements: " << totalElements << std::endl;

    // Use appropriate GGML dequantization function based on type
    switch (tensor->type) {
        case GGML_TYPE_F32: {
            // Already in float format, just copy
            const float* src = (const float*)tensor->data;
            std::memcpy(output.data(), src, totalElements * sizeof(float));
            std::cout << "[INFO]: F32 tensor copied directly" << std::endl;
            break;
        }
        case GGML_TYPE_F16: {
            // Convert from F16 to F32
            const ggml_fp16_t* src = (const ggml_fp16_t*)tensor->data;
            for (size_t i = 0; i < totalElements; ++i) {
                output[i] = ggml_fp16_to_fp32(src[i]);
            }
            std::cout << "[INFO]: F16 tensor converted to F32" << std::endl;
            break;
        }
        case GGML_TYPE_Q4_0: {
            const block_q4_0* blocks = (const block_q4_0*)tensor->data;
            dequantize_row_q4_0(blocks, output.data(), totalElements);
            std::cout << "[INFO]: Q4_0 tensor dequantized" << std::endl;
            break;
        }
        case GGML_TYPE_Q5_1: {
            const block_q5_1* blocks = (const block_q5_1*)tensor->data;
            dequantize_row_q5_1(blocks, output.data(), totalElements);
            std::cout << "[INFO]: Q5_1 tensor dequantized" << std::endl;
            break;
        }
        case GGML_TYPE_Q8_0: {
            const block_q8_0* blocks = (const block_q8_0*)tensor->data;
            dequantize_row_q8_0(blocks, output.data(), totalElements);
            std::cout << "[INFO]: Q8_0 tensor dequantized" << std::endl;
            break;
        }
        default:
            std::cout << "[ERROR]: Unsupported tensor type: " << QuantizationUtils::getTypeName(tensor->type)
                      << " (type=" << (int)tensor->type << ")" << std::endl;
            return E_NOTIMPL;
    }

    std::cout << "[INFO]: Dequantization completed successfully" << std::endl;
    return S_OK;
}

void QuantizationReferenceChecker::cleanup() {
    if (m_whisperContext) {
        whisper_free(m_whisperContext);
        m_whisperContext = nullptr;
    }
    m_initialized = false;
    m_currentModelPath.clear();
}

// Utility functions implementation
namespace QuantizationUtils {
    std::string getTypeName(ggml_type type) {
        switch (type) {
            case GGML_TYPE_F32: return "F32";
            case GGML_TYPE_F16: return "F16";
            case GGML_TYPE_Q4_0: return "Q4_0";
            case GGML_TYPE_Q4_1: return "Q4_1";
            case GGML_TYPE_Q5_0: return "Q5_0";
            case GGML_TYPE_Q5_1: return "Q5_1";
            case GGML_TYPE_Q8_0: return "Q8_0";
            case GGML_TYPE_Q8_1: return "Q8_1";
            default: return "UNKNOWN";
        }
    }

    bool isQuantizedType(ggml_type type) {
        return type >= GGML_TYPE_Q4_0 && type <= GGML_TYPE_Q8_1;
    }

    size_t getBlockSize(ggml_type type) {
        switch (type) {
            case GGML_TYPE_Q4_0: return QK4_0;
            case GGML_TYPE_Q4_1: return QK4_1;
            case GGML_TYPE_Q5_0: return QK5_0;
            case GGML_TYPE_Q5_1: return QK5_1;
            case GGML_TYPE_Q8_0: return QK8_0;
            default: return 1;
        }
    }

    size_t calculateDequantizedSize(size_t quantizedSize, ggml_type type) {
        if (!isQuantizedType(type)) {
            return quantizedSize;
        }
        
        size_t blockSize = getBlockSize(type);
        size_t typeSize = ggml_type_size(type);
        size_t numBlocks = quantizedSize / typeSize;
        
        return numBlocks * blockSize * sizeof(float);
    }
}
