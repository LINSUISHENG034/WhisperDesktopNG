#include "UnifiedTranscriptionParams.h"
#include "../external/whisper.cpp/include/whisper.h"
#include <sstream>
#include <algorithm>
#include <cstring>

namespace Whisper {

// LanguageConfig实现
uint32_t LanguageConfig::toConstMeFormat() const {
    if (code == "auto" || code.empty()) {
        return 0; // Const-me使用0表示自动检测
    }
    return makeLanguageKey(code.c_str());
}

const char* LanguageConfig::toWhisperCppFormat() const {
    if (code == "auto" || code.empty()) {
        return nullptr; // whisper.cpp使用nullptr表示自动检测
    }
    return code.c_str();
}

// UnifiedTranscriptionParams实现

// 从Const-me参数构造
UnifiedTranscriptionParams::UnifiedTranscriptionParams(const sFullParams& constMeParams) {
    // 基础参数映射
    core.strategy = (constMeParams.strategy == eSamplingStrategy::Greedy) ? 
                   UnifiedSamplingStrategy::Greedy : UnifiedSamplingStrategy::BeamSearch;
    core.numThreads = constMeParams.cpuThreads;
    core.maxTextContext = constMeParams.n_max_text_ctx;
    core.offsetMs = constMeParams.offset_ms;
    core.durationMs = constMeParams.duration_ms;
    
    // 语言设置转换
    if (constMeParams.language == 0) {
        core.language.code = "auto";
        core.language.autoDetect = true;
    } else {
        // 从uint32_t转换回语言代码
        char langCode[5] = {0};
        uint32_t lang = constMeParams.language;
        for (int i = 0; i < 4 && lang; i++) {
            langCode[i] = (char)(lang & 0xFF);
            lang >>= 8;
        }
        core.language.code = langCode;
        core.language.autoDetect = false;
    }
    
    // 标志位转换
    core.translate = constMeParams.flag(eFullParamsFlags::Translate);
    core.noContext = constMeParams.flag(eFullParamsFlags::NoContext);
    core.singleSegment = constMeParams.flag(eFullParamsFlags::SingleSegment);
    core.enableTimestamps = !constMeParams.flag(eFullParamsFlags::PrintTimestamps); // 反向逻辑
    core.tokenTimestamps = constMeParams.flag(eFullParamsFlags::TokenTimestamps);
    
    // 时间戳参数
    core.timestampThreshold = constMeParams.thold_pt;
    core.timestampSumThreshold = constMeParams.thold_ptsum;
    core.maxSegmentLength = constMeParams.max_len;
    core.maxTokensPerSegment = constMeParams.max_tokens;
    core.audioContext = constMeParams.audio_ctx;
    
    // 采样参数
    core.greedy.bestOf = constMeParams.greedy.n_past; // 注意：语义可能不完全匹配
    core.beamSearch.beamSize = constMeParams.beam_search.beam_width;
    
    // Const-me特有参数
    constMeAdvanced.newSegmentCallback = constMeParams.new_segment_callback;
    constMeAdvanced.newSegmentCallbackUserData = constMeParams.new_segment_callback_user_data;
    constMeAdvanced.encoderBeginCallback = constMeParams.encoder_begin_callback;
    constMeAdvanced.encoderBeginCallbackUserData = constMeParams.encoder_begin_callback_user_data;
    
    constMeAdvanced.printSpecial = constMeParams.flag(eFullParamsFlags::PrintSpecial);
    constMeAdvanced.printProgress = constMeParams.flag(eFullParamsFlags::PrintProgress);
    constMeAdvanced.printRealtime = constMeParams.flag(eFullParamsFlags::PrintRealtime);
    constMeAdvanced.printTimestamps = constMeParams.flag(eFullParamsFlags::PrintTimestamps);
    constMeAdvanced.speedupAudio = constMeParams.flag(eFullParamsFlags::SpeedupAudio);
}

// 从whisper.cpp参数构造
UnifiedTranscriptionParams::UnifiedTranscriptionParams(const struct whisper_full_params& whisperParams) {
    // 基础参数映射
    core.strategy = (whisperParams.strategy == WHISPER_SAMPLING_GREEDY) ? 
                   UnifiedSamplingStrategy::Greedy : UnifiedSamplingStrategy::BeamSearch;
    core.numThreads = whisperParams.n_threads;
    core.maxTextContext = whisperParams.n_max_text_ctx;
    core.offsetMs = whisperParams.offset_ms;
    core.durationMs = whisperParams.duration_ms;
    
    // 语言设置
    if (whisperParams.language == nullptr || strlen(whisperParams.language) == 0) {
        core.language.code = "auto";
        core.language.autoDetect = true;
    } else {
        core.language.code = whisperParams.language;
        core.language.autoDetect = whisperParams.detect_language;
    }
    
    // 布尔参数
    core.translate = whisperParams.translate;
    core.noContext = whisperParams.no_context;
    core.enableTimestamps = !whisperParams.no_timestamps;
    core.singleSegment = whisperParams.single_segment;
    core.tokenTimestamps = whisperParams.token_timestamps;
    
    // 时间戳参数
    core.timestampThreshold = whisperParams.thold_pt;
    core.timestampSumThreshold = whisperParams.thold_ptsum;
    core.maxSegmentLength = whisperParams.max_len;
    core.maxTokensPerSegment = whisperParams.max_tokens;
    core.audioContext = whisperParams.audio_ctx;
    
    // 采样参数
    core.greedy.bestOf = whisperParams.greedy.best_of;
    core.beamSearch.beamSize = whisperParams.beam_search.beam_size;
    core.beamSearch.patience = whisperParams.beam_search.patience;
    
    // whisper.cpp特有参数
    whisperCppAdvanced.entropyThreshold = whisperParams.entropy_thold;
    whisperCppAdvanced.logprobThreshold = whisperParams.logprob_thold;
    whisperCppAdvanced.noSpeechThreshold = whisperParams.no_speech_thold;
    whisperCppAdvanced.temperature = whisperParams.temperature;
    whisperCppAdvanced.temperatureInc = whisperParams.temperature_inc;
    whisperCppAdvanced.maxInitialTimestamp = whisperParams.max_initial_ts;
    whisperCppAdvanced.lengthPenalty = whisperParams.length_penalty;
    whisperCppAdvanced.suppressBlank = whisperParams.suppress_blank;
    whisperCppAdvanced.suppressNonSpeech = whisperParams.suppress_nst;
    whisperCppAdvanced.debugMode = whisperParams.debug_mode;
    whisperCppAdvanced.enableTinyDiarize = whisperParams.tdrz_enable;
    whisperCppAdvanced.splitOnWord = whisperParams.split_on_word;
    
    if (whisperParams.suppress_regex) {
        whisperCppAdvanced.suppressRegex = whisperParams.suppress_regex;
    }
    if (whisperParams.initial_prompt) {
        whisperCppAdvanced.initialPrompt = whisperParams.initial_prompt;
    }
}

// 转换为Const-me参数
sFullParams UnifiedTranscriptionParams::toConstMeParams() const {
    sFullParams params = {};
    
    // 基础参数
    params.strategy = (core.strategy == UnifiedSamplingStrategy::Greedy) ? 
                     eSamplingStrategy::Greedy : eSamplingStrategy::BeamSearch;
    params.cpuThreads = core.numThreads;
    params.n_max_text_ctx = core.maxTextContext;
    params.offset_ms = core.offsetMs;
    params.duration_ms = core.durationMs;
    params.language = core.language.toConstMeFormat();
    
    // 标志位设置
    params.flags = static_cast<eFullParamsFlags>(0);
    if (core.translate) params.flags |= eFullParamsFlags::Translate;
    if (core.noContext) params.flags |= eFullParamsFlags::NoContext;
    if (core.singleSegment) params.flags |= eFullParamsFlags::SingleSegment;
    if (core.tokenTimestamps) params.flags |= eFullParamsFlags::TokenTimestamps;
    
    // Const-me特有标志位
    if (constMeAdvanced.printSpecial) params.flags |= eFullParamsFlags::PrintSpecial;
    if (constMeAdvanced.printProgress) params.flags |= eFullParamsFlags::PrintProgress;
    if (constMeAdvanced.printRealtime) params.flags |= eFullParamsFlags::PrintRealtime;
    if (constMeAdvanced.printTimestamps) params.flags |= eFullParamsFlags::PrintTimestamps;
    if (constMeAdvanced.speedupAudio) params.flags |= eFullParamsFlags::SpeedupAudio;
    
    // 时间戳参数
    params.thold_pt = core.timestampThreshold;
    params.thold_ptsum = core.timestampSumThreshold;
    params.max_len = core.maxSegmentLength;
    params.max_tokens = core.maxTokensPerSegment;
    params.audio_ctx = core.audioContext;
    
    // 采样参数
    params.greedy.n_past = core.greedy.bestOf;
    params.beam_search.beam_width = core.beamSearch.beamSize;
    
    // 回调函数
    params.new_segment_callback = constMeAdvanced.newSegmentCallback;
    params.new_segment_callback_user_data = constMeAdvanced.newSegmentCallbackUserData;
    params.encoder_begin_callback = constMeAdvanced.encoderBeginCallback;
    params.encoder_begin_callback_user_data = constMeAdvanced.encoderBeginCallbackUserData;
    
    return params;
}

// 转换为whisper.cpp参数
struct whisper_full_params UnifiedTranscriptionParams::toWhisperCppParams() const {
    auto strategy = (core.strategy == UnifiedSamplingStrategy::Greedy) ? 
                   WHISPER_SAMPLING_GREEDY : WHISPER_SAMPLING_BEAM_SEARCH;
    auto params = whisper_full_default_params(strategy);
    
    // 基础参数
    params.n_threads = core.numThreads;
    params.n_max_text_ctx = core.maxTextContext;
    params.offset_ms = core.offsetMs;
    params.duration_ms = core.durationMs;
    
    // 语言设置
    params.language = core.language.toWhisperCppFormat();
    params.detect_language = core.language.getDetectLanguage();
    
    // 布尔参数
    params.translate = core.translate;
    params.no_context = core.noContext;
    params.no_timestamps = !core.enableTimestamps;
    params.single_segment = core.singleSegment;
    params.token_timestamps = core.tokenTimestamps;
    
    // 时间戳参数
    params.thold_pt = core.timestampThreshold;
    params.thold_ptsum = core.timestampSumThreshold;
    params.max_len = core.maxSegmentLength;
    params.max_tokens = core.maxTokensPerSegment;
    params.audio_ctx = core.audioContext;
    
    // 采样参数
    params.greedy.best_of = core.greedy.bestOf;
    params.beam_search.beam_size = core.beamSearch.beamSize;
    params.beam_search.patience = core.beamSearch.patience;
    
    // whisper.cpp特有参数
    params.entropy_thold = whisperCppAdvanced.entropyThreshold;
    params.logprob_thold = whisperCppAdvanced.logprobThreshold;
    params.no_speech_thold = whisperCppAdvanced.noSpeechThreshold;
    params.temperature = whisperCppAdvanced.temperature;
    params.temperature_inc = whisperCppAdvanced.temperatureInc;
    params.max_initial_ts = whisperCppAdvanced.maxInitialTimestamp;
    params.length_penalty = whisperCppAdvanced.lengthPenalty;
    params.suppress_blank = whisperCppAdvanced.suppressBlank;
    params.suppress_nst = whisperCppAdvanced.suppressNonSpeech;
    params.debug_mode = whisperCppAdvanced.debugMode;
    params.tdrz_enable = whisperCppAdvanced.enableTinyDiarize;
    params.split_on_word = whisperCppAdvanced.splitOnWord;
    
    // 字符串参数（需要保持生命周期）
    if (!whisperCppAdvanced.suppressRegex.empty()) {
        params.suppress_regex = whisperCppAdvanced.suppressRegex.c_str();
    }
    if (!whisperCppAdvanced.initialPrompt.empty()) {
        params.initial_prompt = whisperCppAdvanced.initialPrompt.c_str();
    }
    
    return params;
}

// 预设配置
UnifiedTranscriptionParams UnifiedTranscriptionParams::createDefault() {
    UnifiedTranscriptionParams params;
    // 使用默认值，已在结构体中设置
    return params;
}

UnifiedTranscriptionParams UnifiedTranscriptionParams::createHighQuality() {
    auto params = createDefault();
    params.core.strategy = UnifiedSamplingStrategy::BeamSearch;
    params.core.beamSearch.beamSize = 10;
    params.whisperCppAdvanced.temperature = 0.0f;
    params.whisperCppAdvanced.noSpeechThreshold = 0.1f; // 更低的阈值
    return params;
}

UnifiedTranscriptionParams UnifiedTranscriptionParams::createFastTranscription() {
    auto params = createDefault();
    params.core.strategy = UnifiedSamplingStrategy::Greedy;
    params.core.greedy.bestOf = 1;
    params.whisperCppAdvanced.noSpeechThreshold = 0.8f; // 更高的阈值
    return params;
}

UnifiedTranscriptionParams UnifiedTranscriptionParams::createStreamingOptimized() {
    auto params = createDefault();
    params.core.singleSegment = true;
    params.core.noContext = true;
    params.core.audioContext = 512; // 较小的音频上下文
    return params;
}

} // namespace Whisper
