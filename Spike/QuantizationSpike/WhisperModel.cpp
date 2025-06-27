#include "WhisperModel.h"
#include "../../Whisper/source/whisper.h"
#include <iostream>
#include <iomanip>

namespace QuantizationSpike {

// WhisperContextDeleter implementation
void WhisperContextDeleter::operator()(whisper_context* ptr) const {
    if (ptr) {
        whisper_free(ptr);
    }
}

// WhisperCppModel implementation
WhisperCppModel::WhisperCppModel(const std::string& model_path)
    : m_model_path(model_path), m_context(nullptr) {
    
    try {
        validate_file_exists();
        validate_file_format();
        load_model_internal();
        
        std::cout << "Successfully loaded GGML model: " << model_path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load model: " << e.what() << std::endl;
        throw;
    }
}

void WhisperCppModel::validate_file_exists() const {
    if (!std::filesystem::exists(m_model_path)) {
        throw std::runtime_error("Model file not found: " + m_model_path);
    }
    
    auto file_size = std::filesystem::file_size(m_model_path);
    if (file_size < 1024) {  // At least 1KB
        throw std::runtime_error("Model file too small, possibly corrupted: " + m_model_path);
    }
    
    std::cout << "[INFO]: Model file found: " << m_model_path
              << " (Size: " << file_size / 1024 / 1024 << " MB)" << std::endl;
}

void WhisperCppModel::validate_file_format() const {
    std::ifstream file(m_model_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open model file: " + m_model_path);
    }
    
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    
    if (magic != 0x67676d6c) {  // "ggml" in little-endian
        throw std::runtime_error("Invalid GGML magic number in: " + m_model_path + 
                                " (Expected: 0x67676d6c, Got: 0x" + 
                                std::to_string(magic) + ")");
    }
    
    std::cout << "[INFO]: GGML magic number verified: 0x" << std::hex << magic << std::dec << std::endl;
}

void WhisperCppModel::load_model_internal() {
    // Get default parameters
    whisper_context_params cparams = whisper_context_default_params();
    
    // For CPU-only spike, disable GPU
    cparams.use_gpu = false;
    cparams.flash_attn = false;
    
    std::cout << "[INFO]: Loading model with whisper_init_from_file_with_params..." << std::endl;
    
    // Load the model
    m_context = WhisperContextPtr(
        whisper_init_from_file_with_params(m_model_path.c_str(), cparams));

    if (!m_context) {
        throw std::runtime_error("whisper_init_from_file_with_params failed for: " + m_model_path + 
                               ". This could indicate:\n"
                               "  - Unsupported GGML version\n"
                               "  - Corrupted model file\n"
                               "  - Incompatible quantization format\n"
                               "  - Insufficient memory");
    }
}

WhisperCppModel::ModelInfo WhisperCppModel::get_model_info() const {
    if (!m_context) {
        throw std::runtime_error("Model not loaded");
    }
    
    ModelInfo info;
    info.vocab_size = whisper_n_vocab(m_context.get());
    info.n_audio_ctx = whisper_n_audio_ctx(m_context.get());
    info.n_audio_state = whisper_model_n_audio_state(m_context.get());
    info.n_text_ctx = whisper_n_text_ctx(m_context.get());
    info.is_multilingual = whisper_is_multilingual(m_context.get()) != 0;
    info.model_path = m_model_path;
    info.file_size = std::filesystem::file_size(m_model_path);
    
    return info;
}

bool WhisperCppModel::validate_quantization() const {
    try {
        if (!m_context) {
            std::cerr << "❌ Model not loaded" << std::endl;
            return false;
        }
        
        auto info = get_model_info();
        
        std::cout << "Quantization validation:" << std::endl;
        std::cout << "  - Model loaded successfully: YES" << std::endl;
        std::cout << "  - Vocabulary size: " << info.vocab_size << std::endl;
        std::cout << "  - Audio context: " << info.n_audio_ctx << std::endl;
        std::cout << "  - Audio state: " << info.n_audio_state << std::endl;
        std::cout << "  - Text context: " << info.n_text_ctx << std::endl;
        std::cout << "  - Multilingual: " << (info.is_multilingual ? "Yes" : "No") << std::endl;
        
        // Basic validation checks
        bool is_valid = (info.vocab_size > 0 && 
                        info.n_audio_ctx > 0 && 
                        info.n_audio_state > 0 && 
                        info.n_text_ctx > 0);
        
        if (is_valid) {
            std::cout << "Model structure validation: PASSED" << std::endl;
        } else {
            std::cout << "Model structure validation: FAILED" << std::endl;
        }
        
        return is_valid;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Validation error: " << e.what() << std::endl;
        return false;
    }
}

void WhisperCppModel::print_model_info() const {
    try {
        auto info = get_model_info();
        
        std::cout << "\n[INFO]: Model Information:" << std::endl;
        std::cout << "  File: " << info.model_path << std::endl;
        std::cout << "  Size: " << std::fixed << std::setprecision(2)
                  << (info.file_size / 1024.0 / 1024.0) << " MB" << std::endl;
        std::cout << "  Vocabulary: " << info.vocab_size << " tokens" << std::endl;
        std::cout << "  Audio Context: " << info.n_audio_ctx << std::endl;
        std::cout << "  Audio State: " << info.n_audio_state << std::endl;
        std::cout << "  Text Context: " << info.n_text_ctx << std::endl;
        std::cout << "  Multilingual: " << (info.is_multilingual ? "Yes" : "No") << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error getting model info: " << e.what() << std::endl;
    }
}

}
