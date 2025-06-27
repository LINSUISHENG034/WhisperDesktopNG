#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <windows.h>
#include "QuantizationSpike.h"

/**
 * @brief Minimal GGML file parser for spike validation
 * 
 * This parser implements only the essential functionality needed for the spike:
 * - Parse GGML file header (magic number, version)
 * - Find the first quantized tensor (Q4_0, Q5_0, Q8_0)
 * - Extract tensor metadata and data
 * 
 * Note: This is a simplified parser focused on spike goals, not a full implementation.
 */
class MinimalGGMLParser {
public:
    MinimalGGMLParser();
    ~MinimalGGMLParser();

    /**
     * @brief Parse GGML file header and validate format
     * @param filePath Path to the GGML file
     * @return S_OK on success, error HRESULT on failure
     */
    HRESULT parseHeader(const std::string& filePath);

    /**
     * @brief Find the first quantized tensor in the file
     * @param filePath Path to the GGML file
     * @param tensorInfo Output tensor information
     * @return S_OK on success, error HRESULT on failure
     */
    HRESULT findFirstQuantizedTensor(const std::string& filePath, QuantizedTensorInfo& tensorInfo);

    /**
     * @brief Load tensor data from file
     * @param filePath Path to the GGML file
     * @param tensorInfo Tensor information with data offset
     * @return S_OK on success, error HRESULT on failure
     */
    HRESULT loadTensorData(const std::string& filePath, QuantizedTensorInfo& tensorInfo);

    /**
     * @brief Get file format information
     */
    struct GGMLHeader {
        uint32_t magic;
        uint32_t version;
        uint32_t n_vocab;
        uint32_t n_audio_ctx;
        uint32_t n_audio_state;
        uint32_t n_audio_head;
        uint32_t n_audio_layer;
        uint32_t n_text_ctx;
        uint32_t n_text_state;
        uint32_t n_text_head;
        uint32_t n_text_layer;
        uint32_t n_mels;
        uint32_t ftype;
    };

    const GGMLHeader& getHeader() const { return m_header; }

private:
    GGMLHeader m_header;
    bool m_headerParsed;

    /**
     * @brief Check if the tensor type is quantized
     * @param tensorType GGML tensor type
     * @return true if quantized, false otherwise
     */
    bool isQuantizedType(int tensorType);

    /**
     * @brief Get quantization type name for logging
     * @param tensorType GGML tensor type
     * @return String representation of the type
     */
    std::string getQuantizationTypeName(int tensorType);

    /**
     * @brief Calculate tensor data size based on dimensions and type
     * @param dimensions Tensor dimensions
     * @param tensorType GGML tensor type
     * @return Size in bytes
     */
    size_t calculateTensorSize(const std::vector<int>& dimensions, int tensorType);

    /**
     * @brief Read string from file stream
     * @param file File stream
     * @param length String length
     * @return String content
     */
    std::string readString(std::ifstream& file, size_t length);

    /**
     * @brief Read tensor dimensions from file
     * @param file File stream
     * @param dimensions Output dimensions vector
     * @return S_OK on success, error HRESULT on failure
     */
    HRESULT readTensorDimensions(std::ifstream& file, std::vector<int>& dimensions);
};

// GGML constants
#define GGML_MAGIC 0x67676d6c  // "ggml" in little endian
#define GGML_FILE_VERSION 1

// GGML tensor types (focusing on quantized types for spike)
enum ggml_type {
    GGML_TYPE_F32  = 0,
    GGML_TYPE_F16  = 1,
    GGML_TYPE_Q4_0 = 2,
    GGML_TYPE_Q4_1 = 3,
    GGML_TYPE_Q5_0 = 6,
    GGML_TYPE_Q5_1 = 7,
    GGML_TYPE_Q8_0 = 8,
    GGML_TYPE_Q8_1 = 9,
    // Add more as needed for spike
};

// Block sizes for quantized types (bytes per block)
#define GGML_Q4_0_BLOCK_SIZE 18  // 16 4-bit values + 2 bytes scale
#define GGML_Q5_0_BLOCK_SIZE 22  // 16 5-bit values + extras
#define GGML_Q8_0_BLOCK_SIZE 34  // 32 8-bit values + 2 bytes scale
