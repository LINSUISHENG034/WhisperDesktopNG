// 简单的whisper.cpp测试 - 直接使用C API
#include <iostream>
#include <vector>
#include <fstream>
#include <cstdio>

// 直接包含whisper.h
extern "C" {
    #include "../external/whisper.cpp/include/whisper.h"
}

// 简单的WAV文件读取函数
bool read_wav_simple(const char* filename, std::vector<float>& audio) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        printf("ERROR: Cannot open file %s\n", filename);
        return false;
    }
    
    // 跳过WAV头部（44字节）
    file.seekg(44);
    
    // 读取音频数据
    std::vector<int16_t> samples;
    int16_t sample;
    while (file.read(reinterpret_cast<char*>(&sample), sizeof(sample))) {
        samples.push_back(sample);
    }
    
    // 转换为float并归一化
    audio.resize(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        audio[i] = static_cast<float>(samples[i]) / 32768.0f;
    }
    
    printf("Loaded %zu samples from %s\n", audio.size(), filename);
    return true;
}

int main() {
    printf("=== SIMPLE WHISPER.CPP TEST ===\n");
    
    // 1. 初始化whisper上下文参数
    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;  // 使用CPU避免GPU问题
    
    // 2. 加载模型
    printf("Loading model...\n");
    struct whisper_context* ctx = whisper_init_from_file_with_params(
        "E:/Program Files/WhisperDesktop/ggml-tiny.bin", cparams);
    
    if (ctx == nullptr) {
        printf("ERROR: Failed to load model\n");
        return 1;
    }
    printf("Model loaded successfully\n");
    
    // 3. 加载音频文件
    printf("Loading audio file...\n");
    std::vector<float> audio;
    if (!read_wav_simple("external/whisper.cpp/samples/jfk.wav", audio)) {
        whisper_free(ctx);
        return 1;
    }
    
    // 4. 设置转录参数 - 使用最简单的设置
    printf("Setting up parameters...\n");
    struct whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    
    // 使用最基本的参数
    wparams.language = "en";
    wparams.detect_language = false;
    wparams.translate = false;
    wparams.no_timestamps = false;
    wparams.single_segment = false;
    wparams.print_special = false;
    wparams.print_progress = true;
    wparams.print_realtime = false;
    wparams.print_timestamps = true;
    
    // 使用宽松的阈值
    wparams.entropy_thold = 10.0f;
    wparams.logprob_thold = -10.0f;
    wparams.no_speech_thold = 0.1f;
    
    printf("Parameters:\n");
    printf("  strategy=%d\n", wparams.strategy);
    printf("  language=%s\n", wparams.language);
    printf("  entropy_thold=%.2f\n", wparams.entropy_thold);
    printf("  logprob_thold=%.2f\n", wparams.logprob_thold);
    printf("  no_speech_thold=%.2f\n", wparams.no_speech_thold);
    
    // 5. 执行转录
    printf("Starting transcription...\n");
    printf("Audio samples: %zu (%.2f seconds)\n", audio.size(), audio.size() / 16000.0f);
    
    int result = whisper_full(ctx, wparams, audio.data(), static_cast<int>(audio.size()));
    printf("whisper_full returned: %d\n", result);
    
    // 6. 检查结果
    const int n_segments = whisper_full_n_segments(ctx);
    printf("Number of segments: %d\n", n_segments);
    
    if (n_segments > 0) {
        printf("=== TRANSCRIPTION RESULTS ===\n");
        for (int i = 0; i < n_segments; ++i) {
            const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
            const int64_t t1 = whisper_full_get_segment_t1(ctx, i);
            const char* text = whisper_full_get_segment_text(ctx, i);
            
            printf("[%08.3f --> %08.3f] %s\n", 
                   t0 / 100.0f, t1 / 100.0f, text);
        }
        printf("=== SUCCESS ===\n");
    } else {
        printf("=== NO SEGMENTS DETECTED ===\n");
        printf("This indicates the same problem as our main implementation\n");
    }
    
    // 7. 清理
    whisper_free(ctx);
    printf("Test completed\n");
    
    return (n_segments > 0) ? 0 : 1;
}
