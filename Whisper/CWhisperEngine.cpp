/*
 * CWhisperEngine.cpp - CWhisperEngine implementation
 */

#include "stdafx.h"
#include "CWhisperEngine.h"

// Include low-level C API header only in .cpp file
extern "C" {
#include "source/whisper.h"
}

#include <thread> // for std::thread::hardware_concurrency

CWhisperEngine::CWhisperEngine(const std::string& modelPath) {
    // Call C API function to initialize context
    m_ctx = whisper_init(modelPath.c_str());
    if (m_ctx == nullptr) {
        throw CWhisperError("Failed to initialize whisper context from model file: " + modelPath);
    }
}

CWhisperEngine::~CWhisperEngine() {
    if (m_ctx) {
        whisper_free(m_ctx);
    }
}

TranscriptionResult CWhisperEngine::transcribe(const std::vector<float>& audioData) {
    if (!m_ctx) {
        throw CWhisperError("Whisper context is not initialized.");
    }

    // 1. Set transcription parameters
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads = std::max(1, (int)std::thread::hardware_concurrency() / 2);
    params.print_progress = false; // We don't need the library to print progress itself
    params.print_realtime = false; // Avoid real-time printing
    params.print_timestamps = false; // Don't print timestamps to console

    // 2. Call core transcription function
    if (whisper_full(m_ctx, params, audioData.data(), static_cast<int>(audioData.size())) != 0) {
        throw CWhisperError("Failed to process audio data with whisper_full.");
    }

    // 3. Extract results
    TranscriptionResult result;
    const int n_segments = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(m_ctx, i);
        if (text) {
            result.segments.push_back(text);
        }
    }

    result.success = true;
    // (Optional) Get detected language
    // result.detectedLanguage = ...;

    return result;
}
