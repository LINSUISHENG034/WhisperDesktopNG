// 最小化whisper.cpp测试 - 完全模仿官方whisper-cli.exe
#include <iostream>
#include <vector>
#include <cstdio>
#include "../include/whisper_cpp/whisper.h"
#include "../external/whisper.cpp/examples/common.h"

int main() {
    printf("[MINIMAL TEST] Starting whisper.cpp minimal test\n");
    
    // 1. 初始化参数 - 完全模仿官方whisper-cli.exe
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;  // 先用CPU测试，避免GPU问题
    
    // 2. 加载模型
    printf("[MINIMAL TEST] Loading model...\n");
    struct whisper_context* ctx = whisper_init_from_file_with_params("E:/Program Files/WhisperDesktop/ggml-tiny.bin", cparams);
    if (ctx == nullptr) {
        printf("[MINIMAL TEST] ERROR: Failed to load model\n");
        return 1;
    }
    printf("[MINIMAL TEST] Model loaded successfully\n");
    
    // 3. 加载音频文件 - 使用官方的common.h函数
    printf("[MINIMAL TEST] Loading audio file...\n");
    std::vector<float> pcmf32;
    int error = read_wav("external/whisper.cpp/samples/jfk.wav", pcmf32, false);
    if (error != 0) {
        printf("[MINIMAL TEST] ERROR: Failed to load audio file\n");
        whisper_free(ctx);
        return 1;
    }
    printf("[MINIMAL TEST] Audio loaded: %zu samples\n", pcmf32.size());
    
    // 4. 设置转录参数 - 完全使用官方默认值
    printf("[MINIMAL TEST] Setting up transcription parameters...\n");
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    
    // 使用与官方whisper-cli.exe完全相同的参数
    wparams.strategy = WHISPER_SAMPLING_BEAM_SEARCH;
    wparams.beam_search.beam_size = 5;
    wparams.greedy.best_of = 5;
    wparams.n_threads = 4;
    wparams.language = "en";
    wparams.detect_language = false;  // 强制使用英语
    wparams.translate = false;
    wparams.no_timestamps = false;
    wparams.single_segment = false;
    wparams.print_special = false;
    wparams.print_progress = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = true;
    
    // 使用官方默认阈值
    wparams.entropy_thold = 2.40f;
    wparams.logprob_thold = -1.00f;
    wparams.no_speech_thold = 0.60f;
    
    printf("[MINIMAL TEST] Parameters set:\n");
    printf("  strategy=%d, beam_size=%d, best_of=%d\n", wparams.strategy, wparams.beam_search.beam_size, wparams.greedy.best_of);
    printf("  entropy_thold=%.2f, logprob_thold=%.2f, no_speech_thold=%.2f\n", 
           wparams.entropy_thold, wparams.logprob_thold, wparams.no_speech_thold);
    printf("  language=%s, detect_language=%s\n", wparams.language, wparams.detect_language ? "true" : "false");
    
    // 5. 执行转录
    printf("[MINIMAL TEST] Starting transcription...\n");
    int result = whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size());
    printf("[MINIMAL TEST] whisper_full returned: %d\n", result);
    
    // 6. 获取结果
    printf("[MINIMAL TEST] Getting results...\n");
    const int n_segments = whisper_full_n_segments(ctx);
    printf("[MINIMAL TEST] Number of segments: %d\n", n_segments);
    
    if (n_segments > 0) {
        printf("[MINIMAL TEST] SUCCESS! Transcription results:\n");
        for (int i = 0; i < n_segments; ++i) {
            const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
            const int64_t t1 = whisper_full_get_segment_t1(ctx, i);
            const char* text = whisper_full_get_segment_text(ctx, i);
            
            printf("[%08.3f --> %08.3f] %s\n", 
                   t0 / 100.0f, t1 / 100.0f, text);
        }
    } else {
        printf("[MINIMAL TEST] FAILED: No segments detected\n");
        
        // 调试信息
        printf("[MINIMAL TEST] Debug info:\n");
        printf("  Audio samples: %zu\n", pcmf32.size());
        printf("  Audio duration: %.2f seconds\n", pcmf32.size() / 16000.0f);
        printf("  Model type: %s\n", whisper_model_type_readable(ctx));
        printf("  Model language: %s\n", whisper_lang_str(whisper_full_lang_id(ctx)));
    }
    
    // 7. 清理
    whisper_free(ctx);
    printf("[MINIMAL TEST] Test completed\n");
    
    return (n_segments > 0) ? 0 : 1;
}
