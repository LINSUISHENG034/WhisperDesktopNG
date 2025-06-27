#pragma once

#include <string>
#include <memory>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iostream>

// Forward declaration to avoid including whisper.h in header
struct whisper_context;
struct whisper_context_params;

namespace QuantizationSpike {

// Custom deleter for whisper_context
struct WhisperContextDeleter {
    void operator()(whisper_context* ptr) const;
};

using WhisperContextPtr = std::unique_ptr<whisper_context, WhisperContextDeleter>;

class WhisperCppModel {
public:
    // Model information structure
    struct ModelInfo {
        int vocab_size;
        int n_audio_ctx;
        int n_audio_state;
        int n_text_ctx;
        bool is_multilingual;
        std::string model_path;
        size_t file_size;
    };

    explicit WhisperCppModel(const std::string& model_path);
    ~WhisperCppModel() = default;

    // Main functionality
    bool is_loaded() const { return m_context != nullptr; }
    whisper_context* get_context() const { return m_context.get(); }
    
    // Model information and validation
    ModelInfo get_model_info() const;
    bool validate_quantization() const;
    void print_model_info() const;

private:
    std::string m_model_path;
    WhisperContextPtr m_context;
    
    // Helper methods
    void validate_file_exists() const;
    void validate_file_format() const;
    void load_model_internal();
};

}
