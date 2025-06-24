/*
 * CWhisperEngine.cpp - Implementation using latest whisper.cpp API
 */

#include "stdafx.h"
#include "CWhisperEngine.h"
#include "API/sFullParams.h" // Include sProgressSink definition

// Include the latest whisper.cpp API from external submodule
extern "C" {
#include "../external/whisper.cpp/include/whisper.h"
}

#include <thread>
#include <algorithm>
#include <cstring>

CWhisperEngine::CWhisperEngine(const std::string& modelPath, const TranscriptionConfig& config)
    : m_defaultConfig(config), m_isEncoded(false), m_encodedOffset(0) {

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
    : m_ctx(other.m_ctx), m_defaultConfig(std::move(other.m_defaultConfig)),
      m_isEncoded(other.m_isEncoded), m_encodedOffset(other.m_encodedOffset) {
    other.m_ctx = nullptr;
    other.m_isEncoded = false;
    other.m_encodedOffset = 0;
}

CWhisperEngine& CWhisperEngine::operator=(CWhisperEngine&& other) noexcept {
    if (this != &other) {
        if (m_ctx) {
            whisper_free(m_ctx);
        }
        m_ctx = other.m_ctx;
        m_defaultConfig = std::move(other.m_defaultConfig);
        m_isEncoded = other.m_isEncoded;
        m_encodedOffset = other.m_encodedOffset;
        other.m_ctx = nullptr;
        other.m_isEncoded = false;
        other.m_encodedOffset = 0;
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

    // B.1 LOG: CWhisperEngine::transcribe入口 - 打印输入的audioData大小
    printf("[DEBUG] CWhisperEngine::transcribe ENTRY: audioData.size() = %zu\n", audioData.size());

    validateAudioData(audioData);

    // Create whisper parameters using latest API
    auto params = createWhisperParams(config);

    // B.1 LOG: whisper_full调用前 - 打印关键参数
    printf("[DEBUG] CWhisperEngine::transcribe BEFORE whisper_full: language=%s, n_threads=%d, strategy=%d\n",
           params.language, params.n_threads, params.strategy);

    // Reset timings for this transcription
    whisper_reset_timings(m_ctx);

    // Call the core transcription function with latest API
    int whisper_result = whisper_full(m_ctx, params, audioData.data(), static_cast<int>(audioData.size()));

    // B.1 LOG: whisper_full调用后 - 打印返回值和segments数量
    printf("[DEBUG] CWhisperEngine::transcribe AFTER whisper_full: return_code=%d\n", whisper_result);
    if (whisper_result == 0) {
        int n_segments = whisper_full_n_segments(m_ctx);
        printf("[DEBUG] CWhisperEngine::transcribe: whisper_full_n_segments=%d\n", n_segments);
    }

    if (whisper_result != 0) {
        throw CWhisperError("Failed to process audio data with whisper_full.");
    }

    // Extract results using latest API
    TranscriptionResult result = extractResults();

    // B.1 LOG: CWhisperEngine::transcribe出口 - 打印构建的TranscriptionResult中segments数量
    printf("[DEBUG] CWhisperEngine::transcribe EXIT: result.segments.size()=%zu, success=%s\n",
           result.segments.size(), result.success ? "true" : "false");

    return result;
}

TranscriptionResult CWhisperEngine::transcribe(const std::vector<float>& audioData,
                                              const TranscriptionConfig& config,
                                              const Whisper::sProgressSink& progress) {
    if (!m_ctx) {
        throw CWhisperError("Whisper context is not initialized.");
    }

    validateAudioData(audioData);

    // Create whisper parameters using latest API
    auto params = createWhisperParams(config);

    // Set up progress callback if provided
    if (progress.pfn != nullptr) {
        // Create a static wrapper function for the progress callback that matches whisper.cpp's expected signature
        static auto progress_wrapper = [](struct whisper_context* ctx, struct whisper_state* state, int progress_cur, void* user_data) -> void {
            const Whisper::sProgressSink* sink = static_cast<const Whisper::sProgressSink*>(user_data);
            if (sink && sink->pfn) {
                // Convert progress to percentage (0.0 to 1.0)
                double percentage = static_cast<double>(progress_cur) / 100.0;
                // Call the original progress callback
                // Note: We pass nullptr for iContext* since we don't have access to it here
                HRESULT hr = sink->pfn(percentage, nullptr, sink->pv);
                // Note: whisper_progress_callback returns void, so we can't signal cancellation here
                // Cancellation would need to be handled differently if needed
            }
        };
        params.progress_callback = progress_wrapper;
        params.progress_callback_user_data = const_cast<void*>(static_cast<const void*>(&progress));
    }

    // Reset timings for this transcription
    whisper_reset_timings(m_ctx);

    // Call the core transcription function with latest API
    if (whisper_full(m_ctx, params, audioData.data(), static_cast<int>(audioData.size())) != 0) {
        throw CWhisperError("Failed to process audio data with whisper_full.");
    }

    // Extract results using latest API
    return extractResults();
}

bool CWhisperEngine::encode(const std::vector<float>& audioFeatures) {
    if (!m_ctx) {
        throw CWhisperError("Whisper context is not initialized.");
    }

    validateMelData(audioFeatures);

    // Reset timings for this encoding operation
    whisper_reset_timings(m_ctx);

    // Calculate MEL dimensions
    // audioFeatures should be in format [time_steps * N_MEL]
    // N_MEL is typically 80 for Whisper models
    const int N_MEL = 80;
    const int n_len = static_cast<int>(audioFeatures.size()) / N_MEL;

    if (audioFeatures.size() % N_MEL != 0) {
        throw CWhisperError("Audio features size must be divisible by N_MEL (80).");
    }

    // Set the MEL spectrogram data in the whisper context
    if (whisper_set_mel(m_ctx, audioFeatures.data(), n_len, N_MEL) != 0) {
        throw CWhisperError("Failed to set MEL spectrogram data.");
    }

    // Perform encoding with default offset (0) and auto-detected thread count
    const int n_threads = m_defaultConfig.numThreads > 0 ? m_defaultConfig.numThreads :
                         std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);
    const int offset = 0; // Default offset for encoding

    if (whisper_encode(m_ctx, offset, n_threads) != 0) {
        throw CWhisperError("Failed to encode MEL spectrogram data.");
    }

    // Mark as encoded and store the offset used
    m_isEncoded = true;
    m_encodedOffset = offset;

    return true;
}

TranscriptionResult CWhisperEngine::decode() {
    return decode(m_defaultConfig);
}

TranscriptionResult CWhisperEngine::decode(const TranscriptionConfig& config) {
    if (!m_ctx) {
        throw CWhisperError("Whisper context is not initialized.");
    }

    if (!m_isEncoded) {
        throw CWhisperError("decode() called before a successful encode().");
    }

    // Manual sampling decode implementation using verified whisper.cpp APIs
    TranscriptionResult result;
    result.success = false; // Default to failure

    try {
        // Reset timings for decode operation
        whisper_reset_timings(m_ctx);

        // 1. Get model vocabulary size
        const int n_vocab = whisper_model_n_vocab(m_ctx);

        // 2. Language detection and setup
        int lang_id = -1;
        if (config.language != "auto" && !config.language.empty()) {
            lang_id = whisper_lang_id(config.language.c_str());
            if (lang_id < 0) {
                throw CWhisperError("Invalid language: " + config.language);
            }
        } else {
            // Use English as default for auto-detection
            lang_id = whisper_lang_id("en");
        }

        result.detectedLanguageId = lang_id;
        if (lang_id >= 0) {
            const char* lang_str = whisper_lang_str(lang_id);
            if (lang_str) {
                result.detectedLanguage = lang_str;
            }
        }

        // 3. Prepare initial prompt tokens
        std::vector<whisper_token> prompt_tokens;

        // Add start-of-transcript token
        prompt_tokens.push_back(whisper_token_sot(m_ctx));

        // Add language token if specified
        if (lang_id >= 0) {
            prompt_tokens.push_back(whisper_token_lang(m_ctx, lang_id));
        }

        // Add task token (transcribe vs translate)
        if (config.translate) {
            prompt_tokens.push_back(whisper_token_translate(m_ctx));
        } else {
            prompt_tokens.push_back(whisper_token_transcribe(m_ctx));
        }

        // Add timestamp token if timestamps are enabled
        if (config.enableTimestamps) {
            prompt_tokens.push_back(whisper_token_beg(m_ctx));
        }

        // 4. Set decoding loop parameters
        const int max_tokens = 512; // Default maximum tokens for decoding
        const int n_threads = config.numThreads > 0 ? config.numThreads :
                             std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);
        int n_past = 0; // Number of tokens processed

        // 5. Initial decode with prompt tokens
        if (whisper_decode(m_ctx, prompt_tokens.data(), static_cast<int>(prompt_tokens.size()), n_past, n_threads) != 0) {
            throw CWhisperError("whisper_decode failed on initial prompt.");
        }

        n_past += static_cast<int>(prompt_tokens.size());

        // 6. Main decoding loop with manual greedy sampling
        std::string decoded_text;
        int consecutive_timestamps = 0;

        for (int i = 0; i < max_tokens; ++i) {
            // Get logits from the last decode operation
            const float* logits = whisper_get_logits(m_ctx);
            if (!logits) {
                break;
            }

            // Manual greedy sampling: find token with highest probability
            whisper_token best_token_id = 0;
            float max_prob = logits[0];
            for (int j = 1; j < n_vocab; ++j) {
                if (logits[j] > max_prob) {
                    max_prob = logits[j];
                    best_token_id = j;
                }
            }

            // Check for end-of-transcript token
            if (best_token_id == whisper_token_eot(m_ctx)) {
                break;
            }

            // Handle timestamp tokens vs text tokens
            if (best_token_id >= whisper_token_beg(m_ctx)) {
                // This is a timestamp token
                consecutive_timestamps++;
                if (consecutive_timestamps >= 2) {
                    // End of segment detected
                    break;
                }
            } else {
                // This is a text token
                consecutive_timestamps = 0;
                const char* token_str = whisper_token_to_str(m_ctx, best_token_id);
                if (token_str) {
                    decoded_text += token_str;
                }
            }

            // Prepare for next iteration: decode the selected token
            if (whisper_decode(m_ctx, &best_token_id, 1, n_past, n_threads) != 0) {
                break; // Decode failed, stop loop
            }

            n_past++;
        }

        // 7. Create result segment with decoded text
        if (!decoded_text.empty()) {
            TranscriptionResult::Segment segment;
            segment.text = decoded_text;
            segment.startTime = 0;
            segment.endTime = 1000; // Placeholder timestamp
            segment.confidence = 0.8f; // Placeholder confidence
            result.segments.push_back(segment);
        }

        result.success = true;

        // Extract performance timings
        const auto* timings = whisper_get_timings(m_ctx);
        if (timings) {
            result.timings.sampleMs = timings->sample_ms;
            result.timings.encodeMs = timings->encode_ms;
            result.timings.decodeMs = timings->decode_ms;
        }

    } catch (const std::exception&) {
        // Ensure state is reset on exception
        m_isEncoded = false;
        throw; // Re-throw exception
    }

    // Keep encoded state for potential multiple decode calls
    // m_isEncoded remains true to allow multiple decode calls

    return result;
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

    // B.1 LOG: extractResults入口
    printf("[DEBUG] CWhisperEngine::extractResults ENTRY\n");

    // Get number of segments
    const int n_segments = whisper_full_n_segments(m_ctx);
    result.segments.reserve(n_segments);

    // B.1 LOG: 打印segments数量
    printf("[DEBUG] CWhisperEngine::extractResults: n_segments=%d\n", n_segments);

    // Extract language information
    const int lang_id = whisper_full_lang_id(m_ctx);
    if (lang_id >= 0) {
        result.detectedLanguageId = lang_id;
        const char* lang_str = whisper_lang_str(lang_id);
        if (lang_str) {
            result.detectedLanguage = lang_str;
        }
        // B.1 LOG: 打印检测到的语言
        printf("[DEBUG] CWhisperEngine::extractResults: detected language: id=%d, str=%s\n",
               lang_id, lang_str ? lang_str : "NULL");
    }

    // Extract segments with timestamps
    for (int i = 0; i < n_segments; ++i) {
        TranscriptionResult::Segment segment;

        const char* text = whisper_full_get_segment_text(m_ctx, i);
        if (text) {
            segment.text = text;
            // B.1 LOG: 打印每个segment的文本内容
            printf("[DEBUG] CWhisperEngine::extractResults: segment[%d].text=\"%s\"\n", i, text);
        } else {
            printf("[DEBUG] CWhisperEngine::extractResults: segment[%d].text=NULL\n", i);
        }

        // Get timestamps (in centiseconds, convert to milliseconds)
        segment.startTime = whisper_full_get_segment_t0(m_ctx, i) * 10;
        segment.endTime = whisper_full_get_segment_t1(m_ctx, i) * 10;

        // B.1 LOG: 打印时间戳信息
        printf("[DEBUG] CWhisperEngine::extractResults: segment[%d] time: %lld-%lld ms\n",
               i, (long long)segment.startTime, (long long)segment.endTime);

        // Calculate average confidence from tokens in this segment
        const int n_tokens = whisper_full_n_tokens(m_ctx, i);
        if (n_tokens > 0) {
            float total_prob = 0.0f;
            for (int j = 0; j < n_tokens; ++j) {
                total_prob += whisper_full_get_token_p(m_ctx, i, j);
            }
            segment.confidence = total_prob / n_tokens;
        }

        // B.1 LOG: 打印confidence信息
        printf("[DEBUG] CWhisperEngine::extractResults: segment[%d] confidence=%.3f, n_tokens=%d\n",
               i, segment.confidence, n_tokens);

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

    // B.1 LOG: extractResults出口 - 打印最终结果
    printf("[DEBUG] CWhisperEngine::extractResults EXIT: success=%s, segments.size()=%zu\n",
           result.success ? "true" : "false", result.segments.size());

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

void CWhisperEngine::validateMelData(const std::vector<float>& melData) const {
    if (melData.empty()) {
        throw CWhisperError("MEL spectrogram data is empty.");
    }

    // Check that data size is divisible by N_MEL (80)
    const int N_MEL = 80;
    if (melData.size() % N_MEL != 0) {
        throw CWhisperError("MEL data size must be divisible by N_MEL (80).");
    }

    // Calculate time steps
    const int n_len = static_cast<int>(melData.size()) / N_MEL;

    // Check for reasonable MEL length (at least 10 time steps, max 3000 time steps)
    // Each time step represents ~10ms of audio
    const int min_time_steps = 10;   // ~100ms minimum
    const int max_time_steps = 3000; // ~30 seconds maximum

    if (n_len < min_time_steps) {
        throw CWhisperError("MEL data too short (minimum 10 time steps required).");
    }

    if (n_len > max_time_steps) {
        throw CWhisperError("MEL data too long (maximum 3000 time steps supported).");
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
