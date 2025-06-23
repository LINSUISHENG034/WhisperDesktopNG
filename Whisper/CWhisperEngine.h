/*
 * CWhisperEngine.h - Modern C++ wrapper for whisper.cpp interface
 * BACKUP VERSION - This was using old whisper.cpp API
 * NEW VERSION will use external/whisper.cpp/include/whisper.h
 */
#pragma once

#include <string>
#include <vector>
#include <stdexcept>

// Forward declarations of C API structures to avoid exposing low-level details in header
struct whisper_context;
struct whisper_full_params;

// Define a clear structure to hold transcription results
struct TranscriptionResult {
    bool success = false;
    std::string detectedLanguage;
    std::vector<std::string> segments;
    // Future: can add more information like timestamps
};

// CWhisperEngine class declaration
class CWhisperEngine {
public:
    // Constructor: load model, throw exception on failure
    explicit CWhisperEngine(const std::string& modelPath);

    // Destructor: automatically release model resources
    ~CWhisperEngine();

    // Core transcription method: receive audio data, return transcription result
    TranscriptionResult transcribe(const std::vector<float>& audioData);

    // Disable copy constructor and copy assignment to prevent resource management confusion
    CWhisperEngine(const CWhisperEngine&) = delete;
    CWhisperEngine& operator=(const CWhisperEngine&) = delete;

private:
    whisper_context* m_ctx = nullptr;
};

// Custom exception class for reporting engine-related errors
class CWhisperError : public std::runtime_error {
public:
    explicit CWhisperError(const std::string& message) : std::runtime_error(message) {}
};
