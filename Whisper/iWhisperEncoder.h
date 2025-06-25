/*
 * iWhisperEncoder.h - Abstract Encoder Interface
 * 
 * This interface defines the contract for all Whisper encoder implementations.
 * It serves as the "seam" in the Strangler Fig pattern, allowing us to gradually
 * replace the old DirectCompute implementation with the new whisper.cpp implementation.
 * 
 * Design Principles:
 * 1. Interface Segregation: Only essential encoding methods
 * 2. Dependency Inversion: Depend on abstractions, not concretions
 * 3. Open/Closed: Open for extension (new implementations), closed for modification
 */

#pragma once

#include "Whisper/iSpectrogram.h"           // Original project's spectrogram interface
#include "API/iTranscribeResult.cl.h"       // ComLight version of result interface
#include "API/sFullParams.h"                // Progress sink and parameter definitions
#include <windows.h>                        // For HRESULT
#include <memory>                           // For std::unique_ptr
#include <string>                           // For std::string

namespace Whisper
{
    /**
     * @brief Abstract interface for Whisper encoder implementations
     * 
     * This interface abstracts the encoding functionality, allowing different
     * implementations (DirectCompute GPU, whisper.cpp CPU/GPU, etc.) to be
     * used interchangeably through the same interface.
     * 
     * The interface follows the existing patterns in the codebase:
     * - Uses HRESULT for error handling (Windows COM convention)
     * - Accepts iSpectrogram for input (existing interface)
     * - Returns iTranscribeResult for output (existing interface)
     * - Supports progress callbacks for UI integration
     */
    class iWhisperEncoder
    {
    public:
        virtual ~iWhisperEncoder() = default;

        /**
         * @brief Complete transcription with progress support
         * 
         * This is the primary method for full transcription with progress reporting
         * and cancellation support. It performs both encoding and decoding phases.
         * 
         * @param spectrogram Input MEL spectrogram data
         * @param progress Progress reporting and cancellation callbacks
         * @param resultSink Output pointer to store transcription results
         * @return HRESULT S_OK on success, error code on failure
         */
        virtual HRESULT encode(
            iSpectrogram& spectrogram,
            const sProgressSink& progress,
            iTranscribeResult** resultSink
        ) = 0;

        /**
         * @brief Complete transcription without progress callbacks
         * 
         * Simplified version for cases where progress reporting is not needed.
         * Equivalent to calling encode() with empty progress sink.
         * 
         * @param spectrogram Input MEL spectrogram data
         * @param resultSink Output pointer to store transcription results
         * @return HRESULT S_OK on success, error code on failure
         */
        virtual HRESULT encode(
            iSpectrogram& spectrogram,
            iTranscribeResult** resultSink
        ) = 0;

        /**
         * @brief Encode-only operation for streaming pipeline
         * 
         * Performs only the encoding phase, storing the encoded state internally
         * for later decoding. This method is used in streaming scenarios where
         * encoding and decoding are separated.
         * 
         * @param spectrogram Input MEL spectrogram data
         * @param seek Seek offset parameter for audio positioning
         * @return HRESULT S_OK on success, error code on failure
         * 
         * @note Must be followed by a call to decodeOnly() to get results
         */
        virtual HRESULT encodeOnly(
            iSpectrogram& spectrogram,
            int seek
        ) = 0;

        /**
         * @brief Decode-only operation for streaming pipeline
         * 
         * Performs decoding using previously encoded state from encodeOnly().
         * This method completes the streaming pipeline by generating the final
         * transcription results.
         * 
         * @param resultSink Output pointer to store transcription results
         * @return HRESULT S_OK on success, error code on failure
         * 
         * @note Must be preceded by a call to encodeOnly()
         */
        virtual HRESULT decodeOnly(
            iTranscribeResult** resultSink
        ) = 0;

        /**
         * @brief Get implementation information
         * 
         * Returns a string identifying the implementation type.
         * Useful for debugging and logging purposes.
         * 
         * @return const char* Implementation identifier (e.g., "WhisperCpp", "DirectCompute")
         */
        virtual const char* getImplementationName() const = 0;

        /**
         * @brief Check if the encoder is ready for operation
         * 
         * Verifies that the encoder is properly initialized and ready to process
         * audio data. This can be used to validate the encoder state before
         * attempting transcription.
         * 
         * @return bool true if ready, false if not initialized or in error state
         */
        virtual bool isReady() const = 0;
    };

    /**
     * @brief Factory function type for creating encoder instances
     * 
     * This type defines the signature for factory functions that create
     * specific encoder implementations. It allows for dynamic creation
     * of encoders based on configuration or runtime conditions.
     * 
     * @param modelPath Path to the Whisper model file
     * @return std::unique_ptr<iWhisperEncoder> Encoder instance or nullptr on failure
     */
    using EncoderFactory = std::unique_ptr<iWhisperEncoder>(*)(const std::string& modelPath);

} // namespace Whisper
