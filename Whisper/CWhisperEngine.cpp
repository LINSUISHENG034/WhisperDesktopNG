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
#include <numeric>
#include <cstring>
#include <fstream>

// Include audio loading utilities
#include "AudioUtils/common-whisper.h"

CWhisperEngine::CWhisperEngine(const std::string& modelPath, const TranscriptionConfig& config)
    : m_defaultConfig(config), m_isEncoded(false), m_encodedOffset(0) {

    // Create context parameters with latest API
    auto cparams = whisper_context_default_params();
    // CRITICAL FIX: 强制使用CPU模式，避免GPU初始化问题
    cparams.use_gpu = false;  // 强制CPU模式，与官方whisper-cli.exe一致
    cparams.gpu_device = config.gpuDevice;

    // E.2 LOG: 打印context参数设置
    printf("[DEBUG] CWhisperEngine::CWhisperEngine: cparams.use_gpu=%s, gpu_device=%d\n",
           cparams.use_gpu ? "true" : "false", cparams.gpu_device);

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

TranscriptionResult CWhisperEngine::transcribe_DEPRECATED_1(const std::vector<float>& audioData) {
    return transcribe(audioData, m_defaultConfig, Whisper::sProgressSink{});
}

TranscriptionResult CWhisperEngine::transcribe_DEPRECATED_2(const std::vector<float>& audioData,
                                              const TranscriptionConfig& config) {
    // DEPRECATED: Redirect to the main transcribe method with empty progress sink
    return transcribe(audioData, config, Whisper::sProgressSink{});
}

TranscriptionResult CWhisperEngine::transcribe(const std::vector<float>& audioData,
                                              const TranscriptionConfig& config,
                                              const Whisper::sProgressSink& progress) {
    // B.1 LOG: CWhisperEngine::transcribe(progress)入口
    printf("[DEBUG] CWhisperEngine::transcribe [VERSION_NEW_WITH_PROGRESS] ENTRY: audioData.size() = %zu\n", audioData.size());
    printf("[DEBUG] *** E.2 TASK: ABOUT TO CALL createWhisperParams ***\n");
    fflush(stdout);

    if (!m_ctx) {
        throw CWhisperError("Whisper context is not initialized.");
    }

    validateAudioData(audioData);

    // J.1 TASK: 使用官方"黄金标准"参数，完全复制whisper.cpp官方main.cpp的参数创建逻辑
    // 注释掉我们自己的createWhisperParams方法
    // auto params = createWhisperParams(config);

    // 直接复制官方whisper_full_default_params的完整逻辑
    // CRITICAL FIX: 使用BEAM_SEARCH策略，与官方whisper-cli.exe保持一致
    printf("[DEBUG] BEFORE whisper_full_default_params: WHISPER_SAMPLING_BEAM_SEARCH=%d\n", WHISPER_SAMPLING_BEAM_SEARCH);
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    printf("[DEBUG] AFTER whisper_full_default_params: params.strategy=%d\n", params.strategy);

    // 应用官方cli.cpp中的参数设置逻辑
    params.print_realtime   = false;
    params.print_progress   = false;  // 不打印进度
    params.print_timestamps = false; // 不打印时间戳到控制台
    params.print_special    = false;
    params.translate        = config.translate;
    params.language         = config.language.c_str();
    params.detect_language  = (config.language == "auto" || config.language.empty());
    params.n_threads        = config.numThreads > 0 ? config.numThreads :
                              std::min(4, (int32_t) std::thread::hardware_concurrency());

    printf("[DEBUG] *** CHECKPOINT: About to apply CRITICAL FIX ***\n");
    fflush(stdout);

    // CRITICAL FIX: 修正关键参数以确保转录成功
    params.no_context = false;        // 官方默认值是false，不是true！
    params.suppress_blank = false;    // 不抑制空白，让模型自由输出
    params.no_speech_thold = 0.1f;    // 进一步降低阈值，强制语音检测（默认0.6太高）

    // CRITICAL FIX: 使用官方whisper-cli.exe的默认参数值
    params.entropy_thold = 2.40f;     // 官方默认值：2.40
    params.logprob_thold = -1.00f;    // 官方默认值：-1.00
    params.no_speech_thold = 0.60f;   // 官方默认值：0.60
    params.single_segment = false;    // 允许多个分段
    params.max_len = 0;               // 不限制长度
    params.no_timestamps = false;    // 启用时间戳
    params.print_special = false;     // 官方默认值：false

    // CRITICAL FIX: 设置BEAM_SEARCH参数，与官方whisper-cli.exe保持一致
    params.beam_search.beam_size = 5;  // 5 beams，与官方工具一致
    params.greedy.best_of = 5;          // best of 5，与官方工具一致

    printf("[DEBUG] J.1 TASK: Using OFFICIAL DEFAULT whisper.cpp parameters - no_context=%s, suppress_blank=%s, no_speech_thold=%.2f, entropy_thold=%.2f, logprob_thold=%.2f, single_segment=%s\n",
           params.no_context ? "true" : "false",
           params.suppress_blank ? "true" : "false",
           params.no_speech_thold, params.entropy_thold, params.logprob_thold,
           params.single_segment ? "true" : "false");

    // VERIFY: Print parameters again after our modifications
    printf("[DEBUG] VERIFY: After ULTRA AGGRESSIVE modifications - no_context=%s, suppress_blank=%s, no_speech_thold=%.2f, entropy_thold=%.2f, logprob_thold=%.2f, single_segment=%s\n",
           params.no_context ? "true" : "false",
           params.suppress_blank ? "true" : "false",
           params.no_speech_thold, params.entropy_thold, params.logprob_thold,
           params.single_segment ? "true" : "false");

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

    // D.4 TASK: 详细参数对比 - 打印完整的whisper_full_params结构 (progress版本)
    printf("[DEBUG] CWhisperEngine::transcribe(progress) FULL PARAMS:\n");
    printf("  strategy=%d, n_threads=%d, n_max_text_ctx=%d\n", params.strategy, params.n_threads, params.n_max_text_ctx);
    printf("  offset_ms=%d, duration_ms=%d\n", params.offset_ms, params.duration_ms);
    printf("  translate=%s, no_context=%s, no_timestamps=%s\n",
           params.translate ? "true" : "false",
           params.no_context ? "true" : "false",
           params.no_timestamps ? "true" : "false");
    printf("  single_segment=%s, print_special=%s\n",
           params.single_segment ? "true" : "false",
           params.print_special ? "true" : "false");
    printf("  print_progress=%s, print_realtime=%s, print_timestamps=%s\n",
           params.print_progress ? "true" : "false",
           params.print_realtime ? "true" : "false",
           params.print_timestamps ? "true" : "false");
    printf("  token_timestamps=%s, thold_pt=%f, thold_ptsum=%f\n",
           params.token_timestamps ? "true" : "false",
           params.thold_pt, params.thold_ptsum);
    printf("  max_len=%d, split_on_word=%s, max_tokens=%d\n",
           params.max_len, params.split_on_word ? "true" : "false", params.max_tokens);
    printf("  debug_mode=%s, audio_ctx=%d\n",
           params.debug_mode ? "true" : "false", params.audio_ctx);
    printf("  suppress_blank=%s, suppress_nst=%s\n",
           params.suppress_blank ? "true" : "false", params.suppress_nst ? "true" : "false");
    printf("  temperature=%f, max_initial_ts=%f, length_penalty=%f\n",
           params.temperature, params.max_initial_ts, params.length_penalty);
    printf("  temperature_inc=%f, entropy_thold=%f, logprob_thold=%f, no_speech_thold=%f\n",
           params.temperature_inc, params.entropy_thold, params.logprob_thold, params.no_speech_thold);
    printf("  greedy.best_of=%d, beam_search.beam_size=%d, beam_search.patience=%f\n",
           params.greedy.best_of, params.beam_search.beam_size, params.beam_search.patience);
    printf("  language=%s, detect_language=%s\n",
           params.language ? params.language : "NULL", params.detect_language ? "true" : "false");
    fflush(stdout);

    // Reset timings for this transcription
    whisper_reset_timings(m_ctx);

    // CRITICAL DEBUG: 检查音频数据统计 - BEFORE whisper_full
    {
        float min_val = *std::min_element(audioData.begin(), audioData.end());
        float max_val = *std::max_element(audioData.begin(), audioData.end());
        float avg_val = std::accumulate(audioData.begin(), audioData.end(), 0.0f) / audioData.size();

        printf("[DEBUG] CWhisperEngine::transcribe: Audio stats BEFORE whisper_full - min=%.6f, max=%.6f, avg=%.6f, size=%zu\n",
               min_val, max_val, avg_val, audioData.size());
        fflush(stdout);
    }

    // D.2 TASK: 转储音频数据到磁盘用于黄金标准验证
    {
        std::ofstream pcm_dump("dumped_audio_progress.pcm", std::ios::binary);
        if (pcm_dump) {
            pcm_dump.write(reinterpret_cast<const char*>(audioData.data()), audioData.size() * sizeof(float));
            printf("[DEBUG] CWhisperEngine::transcribe(progress): Audio data dumped to dumped_audio_progress.pcm (%zu floats, %zu bytes)\n",
                   audioData.size(), audioData.size() * sizeof(float));
        } else {
            printf("[ERROR] CWhisperEngine::transcribe(progress): Failed to create dumped_audio_progress.pcm\n");
        }
    }

    // CRITICAL FIX: 重置whisper状态，确保干净的开始
    printf("[DEBUG] CWhisperEngine::transcribe: Resetting whisper state...\n");
    whisper_reset_timings(m_ctx);

    // CRITICAL FIX: 验证音频数据长度
    const int audio_len = static_cast<int>(audioData.size());
    printf("[DEBUG] CWhisperEngine::transcribe: Audio length validation - samples=%d, duration=%.2fs\n",
           audio_len, audio_len / 16000.0f);

    if (audio_len < 1600) {  // 至少0.1秒的音频
        printf("[ERROR] CWhisperEngine::transcribe: Audio too short (%d samples)\n", audio_len);
        return TranscriptionResult{};  // 返回空结果
    }

    // Call the core transcription function with latest API
    int whisper_result = whisper_full(m_ctx, params, audioData.data(), audio_len);

    // B.1 LOG: 打印whisper_full返回值和音频数据统计
    printf("[DEBUG] CWhisperEngine::transcribe: whisper_full returned %d\n", whisper_result);

    // 检查音频数据统计 - AFTER whisper_full
    float min_val_after = *std::min_element(audioData.begin(), audioData.end());
    float max_val_after = *std::max_element(audioData.begin(), audioData.end());
    float avg_val_after = std::accumulate(audioData.begin(), audioData.end(), 0.0f) / audioData.size();

    printf("[DEBUG] CWhisperEngine::transcribe: Audio stats AFTER whisper_full - min=%.6f, max=%.6f, avg=%.6f, size=%zu\n",
           min_val_after, max_val_after, avg_val_after, audioData.size());
    fflush(stdout);

    if (whisper_result != 0) {
        printf("[ERROR] CWhisperEngine::transcribe: whisper_full failed with code %d\n", whisper_result);
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
    // 基于官方文档的参数优化
    auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    printf("[DEBUG] CWhisperEngine::createWhisperParams: Using optimized parameters based on official docs\n");

    // Thread configuration
    params.n_threads = config.numThreads > 0 ? config.numThreads :
                      std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);

    // E.2 TASK: 设置beam search参数以对齐黄金标准
    params.beam_search.beam_size = 5;  // 对齐黄金标准的"5 beams"
    params.greedy.best_of = 5;          // 对齐黄金标准的"best of 5"

    // E.2 LOG: 打印beam search参数
    printf("[DEBUG] CWhisperEngine::createWhisperParams: strategy=%d, beam_size=%d, best_of=%d\n",
           params.strategy, params.beam_search.beam_size, params.greedy.best_of);

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

    // Quality settings - 基于官方文档优化
    params.temperature = 0.0f;  // 使用确定性输出
    params.suppress_blank = false;  // 关键修改：不抑制空白，让模型自由输出
    params.suppress_nst = false;  // 不抑制非语音token

    printf("[DEBUG] CWhisperEngine::createWhisperParams: CRITICAL - suppress_blank=false for better detection\n");

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

    // Context settings - H.1 FIX: Set no_context to false to enable transcription
    // The issue was that no_context=true prevents the model from generating segments
    params.no_context = false;  // Enable context for proper transcription

    // CRITICAL FIX: 调整语音检测阈值 - 这是关键参数！
    params.no_speech_thold = 0.3f;  // 降低阈值，提高语音检测敏感度（默认0.6）

    printf("[DEBUG] CWhisperEngine::createWhisperParams: CRITICAL - no_speech_thold=%.2f (lowered for better detection)\n",
           params.no_speech_thold);

    // H.1 DEBUG: Verify the no_context setting
    printf("[DEBUG] CWhisperEngine::createWhisperParams: FINAL no_context=%s\n",
           params.no_context ? "true" : "false");

    return params;
}

TranscriptionResult CWhisperEngine::extractResults() const {
    TranscriptionResult result;

    // B.1 LOG: extractResults入口
    printf("[DEBUG] CWhisperEngine::extractResults ENTRY\n");

    // Get number of segments
    // B.1 LOG: 检查m_ctx状态
    printf("[DEBUG] CWhisperEngine::extractResults: m_ctx=%p\n", m_ctx);
    fflush(stdout);

    const int n_segments = whisper_full_n_segments(m_ctx);

    // B.1 LOG: 打印segments数量和详细信息
    printf("[DEBUG] CWhisperEngine::extractResults: whisper_full_n_segments returned %d\n", n_segments);
    printf("[DEBUG] CWhisperEngine::extractResults: m_ctx after call=%p\n", m_ctx);
    fflush(stdout);

    if (n_segments < 0) {
        printf("[ERROR] CWhisperEngine::extractResults: whisper_full_n_segments returned error code %d\n", n_segments);
        result.success = false;
        return result;
    }

    result.segments.reserve(n_segments);

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

// NEW: Direct PCM transcription method - using audio file path
TranscriptionResult CWhisperEngine::transcribeFromFile(const std::string& audioFilePath,
                                                       const TranscriptionConfig& config,
                                                       const Whisper::sProgressSink& progress)
{
    printf("[DEBUG] CWhisperEngine::transcribeFromFile ENTRY: %s\n", audioFilePath.c_str());
    fflush(stdout);

    TranscriptionResult result;

    try {
        // Load PCM audio data directly from file using official whisper.cpp function
        std::vector<float> pcmData;
        std::vector<std::vector<float>> pcmStereoData;

        printf("[DEBUG] CWhisperEngine::transcribeFromFile: Loading audio file...\n");
        fflush(stdout);

        if (!read_audio_data(audioFilePath, pcmData, pcmStereoData, false)) {
            printf("[ERROR] CWhisperEngine::transcribeFromFile: Failed to load audio file\n");
            fflush(stdout);
            return result; // Return empty result on failure
        }

        printf("[DEBUG] CWhisperEngine::transcribeFromFile: Loaded %zu PCM samples\n", pcmData.size());
        fflush(stdout);

        // Check PCM data statistics
        if (!pcmData.empty()) {
            float min_val = *std::min_element(pcmData.begin(), pcmData.end());
            float max_val = *std::max_element(pcmData.begin(), pcmData.end());
            float avg_val = std::accumulate(pcmData.begin(), pcmData.end(), 0.0f) / pcmData.size();

            printf("[DEBUG] CWhisperEngine::transcribeFromFile: PCM stats - min=%.6f, max=%.6f, avg=%.6f, size=%zu\n",
                   min_val, max_val, avg_val, pcmData.size());
            fflush(stdout);
        }

        // Set up whisper parameters
        whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

        // Apply our configuration
        params.language = config.language.empty() ? "auto" : config.language.c_str();
        params.translate = config.translate;
        params.print_progress = false;
        params.print_timestamps = false;
        params.print_realtime = false;
        params.print_special = false;

        // CRITICAL FIX: 修正关键参数以确保转录成功
        params.no_context = false;        // 官方默认值是false，不是true！
        params.suppress_blank = false;    // 不抑制空白，让模型自由输出
        params.no_speech_thold = 0.1f;    // 进一步降低阈值，强制语音检测（默认0.6太高）

        // ULTRA AGGRESSIVE FIX: 完全禁用过滤，强制输出
        params.entropy_thold = 100.0f;    // 极高熵阈值，禁用熵过滤
        params.logprob_thold = -100.0f;   // 极低对数概率阈值，禁用概率过滤
        params.single_segment = false;    // 允许多个分段
        params.max_len = 0;               // 不限制长度
        params.no_timestamps = false;    // 启用时间戳
        params.print_special = true;      // 打印特殊标记

        printf("[DEBUG] CWhisperEngine::transcribeFromFile: Using ULTRA AGGRESSIVE whisper.cpp parameters - no_context=%s, suppress_blank=%s, no_speech_thold=%.2f, entropy_thold=%.2f, logprob_thold=%.2f, single_segment=%s\n",
               params.no_context ? "true" : "false",
               params.suppress_blank ? "true" : "false",
               params.no_speech_thold, params.entropy_thold, params.logprob_thold,
               params.single_segment ? "true" : "false");
        fflush(stdout);

        // Call whisper_full with PCM data
        printf("[DEBUG] CWhisperEngine::transcribeFromFile: Calling whisper_full with %zu PCM samples...\n", pcmData.size());
        fflush(stdout);

        int result_code = whisper_full(m_ctx, params, pcmData.data(), static_cast<int>(pcmData.size()));

        printf("[DEBUG] CWhisperEngine::transcribeFromFile: whisper_full returned: %d\n", result_code);
        fflush(stdout);

        if (result_code != 0) {
            printf("[ERROR] CWhisperEngine::transcribeFromFile: whisper_full failed with code %d\n", result_code);
            fflush(stdout);
            return result; // Return empty result on failure
        }

        // Extract results
        result = extractResults();

        printf("[DEBUG] CWhisperEngine::transcribeFromFile: Extracted %zu segments\n", result.segments.size());
        fflush(stdout);

        return result;
    }
    catch (const std::exception& e) {
        printf("[ERROR] CWhisperEngine::transcribeFromFile: Exception: %s\n", e.what());
        fflush(stdout);
        return result; // Return empty result on exception
    }
}

// Static progress callback function for whisper.cpp
static void whisper_progress_callback_impl(struct whisper_context* /*ctx*/, struct whisper_state* /*state*/, int progress, void* user_data) {
    const Whisper::sProgressSink* sink = static_cast<const Whisper::sProgressSink*>(user_data);
    if (sink && sink->pfn) {
        float progressFloat = static_cast<float>(progress) / 100.0f;
        HRESULT hr = sink->pfn(progressFloat, nullptr, sink->pv);
        // Note: whisper_progress_callback doesn't support cancellation via return value
        // The original callback returns HRESULT but we can't use it here
    }
}

// NEW: Direct MEL transcription method
TranscriptionResult CWhisperEngine::transcribeFromMel(const std::vector<float>& melData,
                                                     size_t melLength,
                                                     const TranscriptionConfig& config,
                                                     const Whisper::sProgressSink& progress) {
    printf("[DEBUG] CWhisperEngine::transcribeFromMel ENTRY: melData.size()=%zu, melLength=%zu\n",
           melData.size(), melLength);
    fflush(stdout);

    TranscriptionResult result;
    result.success = false;

    if (!m_ctx) {
        printf("[ERROR] CWhisperEngine::transcribeFromMel: Context not initialized\n");
        return result;
    }

    try {
        // 1. Set MEL data directly into whisper context
        const int n_mel = 80; // Whisper standard MEL bands
        if (whisper_set_mel(m_ctx, melData.data(), static_cast<int>(melLength), n_mel) != 0) {
            printf("[ERROR] CWhisperEngine::transcribeFromMel: whisper_set_mel failed\n");
            return result;
        }

        printf("[DEBUG] CWhisperEngine::transcribeFromMel: MEL data set successfully\n");

        // Debug: Check MEL data statistics
        float minVal = *std::min_element(melData.begin(), melData.end());
        float maxVal = *std::max_element(melData.begin(), melData.end());
        float avgVal = std::accumulate(melData.begin(), melData.end(), 0.0f) / melData.size();
        printf("[DEBUG] CWhisperEngine::transcribeFromMel: MEL stats - min=%.6f, max=%.6f, avg=%.6f\n",
               minVal, maxVal, avgVal);

        // Calculate expected audio duration
        float audioDurationSec = (float)melLength * 0.02f; // Each MEL frame = 20ms
        printf("[DEBUG] CWhisperEngine::transcribeFromMel: Expected audio duration: %.2f seconds\n", audioDurationSec);

        // 2. Create whisper parameters
        whisper_full_params params = createWhisperParams(config);

        // 3. Set up progress callback if provided
        if (progress.pfn) {
            // Use static function for progress callback
            params.progress_callback = whisper_progress_callback_impl;
            params.progress_callback_user_data = const_cast<void*>(static_cast<const void*>(&progress));
        }

        // 4. Run full transcription pipeline (encode + decode)
        printf("[DEBUG] CWhisperEngine::transcribeFromMel: Starting whisper_full\n");
        if (whisper_full(m_ctx, params, nullptr, 0) != 0) {
            printf("[ERROR] CWhisperEngine::transcribeFromMel: whisper_full failed\n");
            return result;
        }

        // 5. Extract results
        result = extractResults();
        printf("[DEBUG] CWhisperEngine::transcribeFromMel: Transcription completed, segments=%zu\n",
               result.segments.size());

        return result;
    }
    catch (const std::exception& e) {
        printf("[ERROR] CWhisperEngine::transcribeFromMel: Exception: %s\n", e.what());
        fflush(stdout);
        return result; // Return empty result on exception
    }
}
