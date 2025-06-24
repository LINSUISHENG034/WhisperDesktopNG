/*
 * CWhisperEngine.h - Modern C++ wrapper for latest whisper.cpp interface
 * Uses external/whisper.cpp/include/whisper.h (latest ggerganov/whisper.cpp)
 * Supports quantized models and latest features
 */
#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <memory>

// Forward declarations of C API structures to avoid exposing low-level details in header
struct whisper_context;
struct whisper_context_params;
struct whisper_full_params;

// Enhanced structure to hold transcription results with timestamps and language detection
struct TranscriptionResult {
    bool success = false;
    std::string detectedLanguage;
    int detectedLanguageId = -1;

    struct Segment {
        std::string text;
        int64_t startTime = 0;  // in milliseconds
        int64_t endTime = 0;    // in milliseconds
        float confidence = 0.0f;
    };

    std::vector<Segment> segments;

    // Performance information
    struct Timings {
        float sampleMs = 0.0f;
        float encodeMs = 0.0f;
        float decodeMs = 0.0f;
    } timings;
};

// Configuration for transcription
struct TranscriptionConfig {
    // Language settings
    std::string language = "auto";  // "auto" for auto-detection, or specific language code
    bool translate = false;         // translate to English

    // Quality settings
    bool enableTimestamps = true;   // extract segment timestamps
    bool enableTokenTimestamps = false; // token-level timestamps (experimental)

    // Performance settings
    int numThreads = 0;            // 0 = auto-detect
    bool useGpu = false;           // enable GPU acceleration if available
    int gpuDevice = 0;             // CUDA device ID

    // Advanced settings
    float temperature = 0.0f;      // sampling temperature
    bool suppressBlank = true;     // suppress blank outputs
    bool suppressNonSpeech = true; // suppress non-speech tokens

    // Experimental features
    bool enableVAD = false;        // Voice Activity Detection
    std::string vadModelPath;      // Path to VAD model
};

// Modern C++ wrapper for whisper.cpp with RAII and exception safety
class CWhisperEngine {
public:
    // Constructor: load model with configuration, throw exception on failure
    explicit CWhisperEngine(const std::string& modelPath,
                           const TranscriptionConfig& config = TranscriptionConfig{});

    // Destructor: automatically release model resources
    ~CWhisperEngine();

    // Core transcription method: receive audio data, return transcription result
    TranscriptionResult transcribe(const std::vector<float>& audioData);

    // Transcribe with custom configuration (overrides constructor config)
    TranscriptionResult transcribe(const std::vector<float>& audioData,
                                   const TranscriptionConfig& config);

    // Separate encoding and decoding methods for streaming pipeline integration

    // Encode method: process MEL spectrogram data and store encoded state
    // Input: MEL spectrogram data (audioFeatures should be in time-first format: [time_steps * N_MEL])
    // Returns: true on success, false on failure
    // Note: Encoded state is stored internally in whisper_context for later decoding
    bool encode(const std::vector<float>& audioFeatures);

    // Decode method: perform decoding using previously encoded state
    // Returns: TranscriptionResult with decoded text segments
    // Note: Must call encode() first to establish the encoded state
    TranscriptionResult decode();

    // Decode with custom configuration (overrides constructor config)
    TranscriptionResult decode(const TranscriptionConfig& config);

    // Get model information
    std::string getModelType() const;
    bool isMultilingual() const;
    std::vector<std::string> getAvailableLanguages() const;

    // Performance monitoring
    void resetTimings();
    void printTimings() const;

    // Disable copy constructor and copy assignment to prevent resource management confusion
    CWhisperEngine(const CWhisperEngine&) = delete;
    CWhisperEngine& operator=(const CWhisperEngine&) = delete;

    // Enable move semantics for efficient resource transfer
    CWhisperEngine(CWhisperEngine&& other) noexcept;
    CWhisperEngine& operator=(CWhisperEngine&& other) noexcept;

private:
    whisper_context* m_ctx = nullptr;
    TranscriptionConfig m_defaultConfig;

    // State tracking for separate encode/decode operations
    bool m_isEncoded = false;  // Track whether encoding has been performed
    int m_encodedOffset = 0;   // Store the offset used during encoding

    // Helper methods
    whisper_full_params createWhisperParams(const TranscriptionConfig& config) const;
    TranscriptionResult extractResults() const;
    void validateAudioData(const std::vector<float>& audioData) const;
    void validateMelData(const std::vector<float>& melData) const;
};

// Custom exception class for reporting engine-related errors
class CWhisperError : public std::runtime_error {
public:
    explicit CWhisperError(const std::string& message) : std::runtime_error(message) {}
};
