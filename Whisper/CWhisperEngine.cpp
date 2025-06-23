/*
 * CWhisperEngine.cpp - Implementation using latest whisper.cpp API
 */

#include "stdafx.h"
#include "CWhisperEngine.h"

// Include the latest whisper.cpp API
extern "C" {
#include "whisper.h"
}

#include <thread>
#include <algorithm>
#include <cstring>

CWhisperEngine::CWhisperEngine(const std::string& modelPath, const TranscriptionConfig& config)
    : m_defaultConfig(config) {

    // Create context parameters with latest API
    auto cparams = whisper_context_default_params();
    cparams.use_gpu = config.useGpu;
    cparams.gpu_device = config.gpuDevice;

    // Initialize context with new API
    m_ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
    if (m_ctx == nullptr) {
        throw CWhisperError("Failed to initialize whisper context from model file: " + modelPath);
    }
}

CWhisperEngine::~CWhisperEngine() {
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
    }
}

CWhisperEngine::CWhisperEngine(CWhisperEngine&& other) noexcept
    : m_ctx(other.m_ctx), m_defaultConfig(std::move(other.m_defaultConfig)) {
    other.m_ctx = nullptr;
}

CWhisperEngine& CWhisperEngine::operator=(CWhisperEngine&& other) noexcept {
    if (this != &other) {
        if (m_ctx) {
            whisper_free(m_ctx);
        }
        m_ctx = other.m_ctx;
        m_defaultConfig = std::move(other.m_defaultConfig);
        other.m_ctx = nullptr;
    }
    return *this;
}

TranscriptionResult CWhisperEngine::transcribe(const std::vector<float>& audioData) {
    return transcribe(audioData, m_defaultConfig);
}

TranscriptionResult CWhisperEngine::transcribe(const std::vector<float>& audioData,
                                              const TranscriptionConfig& config) {
    if (!m_ctx) {
        throw CWhisperError("Whisper context is not initialized.");
    }

    validateAudioData(audioData);

    // Create whisper parameters using latest API
    auto params = createWhisperParams(config);

    // Reset timings for this transcription
    whisper_reset_timings(m_ctx);

    // Call the core transcription function with latest API
    if (whisper_full(m_ctx, params, audioData.data(), static_cast<int>(audioData.size())) != 0) {
        throw CWhisperError("Failed to process audio data with whisper_full.");
    }

    // Extract results using latest API
    return extractResults();
}

whisper_full_params CWhisperEngine::createWhisperParams(const TranscriptionConfig& config) const {
    auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    // Thread configuration
    params.n_threads = config.numThreads > 0 ? config.numThreads :
                      std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);

    // Language settings
    if (config.language != "auto" && !config.language.empty()) {
        params.language = config.language.c_str();
        params.detect_language = false;
    } else {
        params.language = nullptr;
        params.detect_language = true;
    }

    params.translate = config.translate;

    // Timestamp settings
    params.no_timestamps = !config.enableTimestamps;
    params.token_timestamps = config.enableTokenTimestamps;

    // Quality settings
    params.temperature = config.temperature;
    params.suppress_blank = config.suppressBlank;
    params.suppress_nst = config.suppressNonSpeech;

    // Disable console output - we handle results programmatically
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.print_special = false;

    // VAD settings
    params.vad = config.enableVAD;
    if (config.enableVAD && !config.vadModelPath.empty()) {
        params.vad_model_path = config.vadModelPath.c_str();
    }

    return params;
}

TranscriptionResult CWhisperEngine::extractResults() const {
    TranscriptionResult result;

    // Get number of segments
    const int n_segments = whisper_full_n_segments(m_ctx);
    result.segments.reserve(n_segments);

    // Extract language information
    const int lang_id = whisper_full_lang_id(m_ctx);
    if (lang_id >= 0) {
        result.detectedLanguageId = lang_id;
        const char* lang_str = whisper_lang_str(lang_id);
        if (lang_str) {
            result.detectedLanguage = lang_str;
        }
    }

    // Extract segments with timestamps
    for (int i = 0; i < n_segments; ++i) {
        TranscriptionResult::Segment segment;

        const char* text = whisper_full_get_segment_text(m_ctx, i);
        if (text) {
            segment.text = text;
        }

        // Get timestamps (in centiseconds, convert to milliseconds)
        segment.startTime = whisper_full_get_segment_t0(m_ctx, i) * 10;
        segment.endTime = whisper_full_get_segment_t1(m_ctx, i) * 10;

        // Calculate average confidence from tokens in this segment
        const int n_tokens = whisper_full_n_tokens(m_ctx, i);
        if (n_tokens > 0) {
            float total_prob = 0.0f;
            for (int j = 0; j < n_tokens; ++j) {
                total_prob += whisper_full_get_token_p(m_ctx, i, j);
            }
            segment.confidence = total_prob / n_tokens;
        }

        result.segments.push_back(segment);
    }

    // Extract performance timings
    const auto* timings = whisper_get_timings(m_ctx);
    if (timings) {
        result.timings.sampleMs = timings->sample_ms;
        result.timings.encodeMs = timings->encode_ms;
        result.timings.decodeMs = timings->decode_ms;
    }

    result.success = true;
    return result;
}

void CWhisperEngine::validateAudioData(const std::vector<float>& audioData) const {
    if (audioData.empty()) {
        throw CWhisperError("Audio data is empty.");
    }

    // Check for reasonable audio length (at least 100ms at 16kHz)
    const size_t min_samples = 1600; // 100ms at 16kHz
    if (audioData.size() < min_samples) {
        throw CWhisperError("Audio data too short (minimum 100ms required).");
    }

    // Check for reasonable maximum length (10 minutes at 16kHz)
    const size_t max_samples = 16000 * 60 * 10; // 10 minutes at 16kHz
    if (audioData.size() > max_samples) {
        throw CWhisperError("Audio data too long (maximum 10 minutes supported).");
    }
}

std::string CWhisperEngine::getModelType() const {
    if (!m_ctx) {
        throw CWhisperError("Whisper context is not initialized.");
    }

    const char* type_str = whisper_model_type_readable(m_ctx);
    return type_str ? std::string(type_str) : "unknown";
}

bool CWhisperEngine::isMultilingual() const {
    if (!m_ctx) {
        throw CWhisperError("Whisper context is not initialized.");
    }

    return whisper_is_multilingual(m_ctx) != 0;
}

std::vector<std::string> CWhisperEngine::getAvailableLanguages() const {
    std::vector<std::string> languages;

    const int max_lang_id = whisper_lang_max_id();
    for (int i = 0; i <= max_lang_id; ++i) {
        const char* lang_str = whisper_lang_str(i);
        if (lang_str) {
            languages.emplace_back(lang_str);
        }
    }

    return languages;
}

void CWhisperEngine::resetTimings() {
    if (m_ctx) {
        whisper_reset_timings(m_ctx);
    }
}

void CWhisperEngine::printTimings() const {
    if (m_ctx) {
        whisper_print_timings(m_ctx);
    }
}
