/*
 * WhisperCppEncoder.h - Adapter Interface
 * This class serves as a bridge between new and old code.
 * Adapts the new CWhisperEngine with the original project's iSpectrogram and iTranscribeResult interfaces
 */
#pragma once

#include "CWhisperEngine.h" // Include our own engine
#include "Whisper/iSpectrogram.h" // Include original project's spectrogram interface
#include "API/iTranscribeResult.cl.h" // Include ComLight version of result interface
#include "API/TranscribeStructs.h" // Include result data structures
#include "API/sFullParams.h" // Include sProgressSink definition
#include "Whisper/TranscribeResult.h" // Include TranscribeResult implementation class

namespace Whisper
{
    // Adapter class: connects new CWhisperEngine with original Whisper interfaces
    class WhisperCppEncoder {
    public:
        // Constructor: receives model path and creates CWhisperEngine internally
        WhisperCppEncoder(const std::string& modelPath);

        // Constructor with configuration
        WhisperCppEncoder(const std::string& modelPath, const TranscriptionConfig& config);

        // Destructor
        ~WhisperCppEncoder();

        // Core "encode" method
        // Will be called by ContextImpl
        // HRESULT is a common Windows return type indicating success or failure
        HRESULT encode(
            iSpectrogram& spectrogram,      // Input: original project's spectrogram object
            iTranscribeResult** resultSink  // Output: pointer to store result object
        );

        // Enhanced "encode" method with progress callback support
        // This version supports progress reporting and cancellation
        HRESULT encode(
            iSpectrogram& spectrogram,      // Input: original project's spectrogram object
            const sProgressSink& progress,  // Input: progress reporting and cancellation callbacks
            iTranscribeResult** resultSink  // Output: pointer to store result object
        );

        // Encode-only method that matches ContextImpl::encode signature
        // This method only performs encoding without full transcription
        HRESULT encodeOnly(
            iSpectrogram& spectrogram,      // Input: original project's spectrogram object
            int seek                        // Seek offset parameter
        );

        // Decode-only method that performs decoding after encoding
        // This method uses the previously encoded state to generate transcription results
        HRESULT decodeOnly(
            iTranscribeResult** resultSink  // Output: pointer to store result object
        );

        // Get engine information
        std::string getModelType() const;
        bool isMultilingual() const;

        // Disable copy constructor and assignment
        WhisperCppEncoder(const WhisperCppEncoder&) = delete;
        WhisperCppEncoder& operator=(const WhisperCppEncoder&) = delete;

    private:
        // Hold our new engine instance
        std::unique_ptr<CWhisperEngine> m_engine;

        // Configuration information
        TranscriptionConfig m_config;

        // Private helper methods

        // Data conversion: from iSpectrogram -> std::vector<float>
        HRESULT extractMelData(iSpectrogram& spectrogram, std::vector<float>& audioFeatures);

        // Result conversion: from TranscriptionResult -> TranscribeResult
        HRESULT convertResults(const TranscriptionResult& engineResult, TranscribeResult& result);

        // Time conversion: from milliseconds to 100-nanosecond ticks
        uint64_t millisecondsToTicks(int64_t milliseconds) const;

        // String management: ensure strings remain valid during result object lifetime
        void manageResultStrings(TranscribeResult& result, const TranscriptionResult& engineResult);
    };
}
