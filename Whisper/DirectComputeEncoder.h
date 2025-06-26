/*
 * DirectComputeEncoder.h - DirectCompute Implementation Wrapper
 * 
 * This class wraps the original DirectCompute-based Whisper implementation
 * to conform to the iWhisperEncoder interface. It serves as the "legacy"
 * implementation in the Strangler Fig pattern.
 * 
 * Design Purpose:
 * 1. Preserve existing DirectCompute functionality
 * 2. Provide interface compatibility with iWhisperEncoder
 * 3. Enable gradual migration to whisper.cpp implementation
 * 4. Maintain performance characteristics of GPU acceleration
 */

#pragma once

#include "iWhisperEncoder.h"                // Abstract interface
#include "Whisper/WhisperContext.h"         // DirectCompute context
#include "Whisper/WhisperModel.h"           // Model data structures
#include "Whisper/sEncodeParams.h"          // Encode/decode parameter structures
#include "ML/Device.h"                      // DirectCompute device
#include "Utils/ProfileCollection.h"       // Performance profiling
#include "Whisper/TranscribeResult.h"       // Result implementation
#include "API/sFullParams.h"                // Parameter structures
#include <memory>

namespace Whisper
{
    /**
     * @brief DirectCompute-based implementation of iWhisperEncoder
     * 
     * This class wraps the original DirectCompute implementation to provide
     * interface compatibility. It maintains the existing GPU-accelerated
     * processing pipeline while conforming to the new interface contract.
     * 
     * Key Features:
     * - GPU acceleration via DirectCompute shaders
     * - Streaming encode/decode separation
     * - Performance profiling integration
     * - Compatible with existing model loading
     */
    class DirectComputeEncoder : public iWhisperEncoder
    {
    private:
        // Core DirectCompute components
        const DirectCompute::Device& m_device;
        const WhisperModel& m_model;
        DirectCompute::WhisperContext m_context;
        ProfileCollection m_profiler;
        
        // State management for streaming operations
        bool m_isEncoded;
        int m_lastSeek;
        
        // Probability buffer for decoding
        std::vector<float> m_probs;
        
        // Result storage for streaming operations
        ComLight::CComPtr<iTranscribeResult> m_lastResult;

    public:
        /**
         * @brief Constructor
         * 
         * @param device DirectCompute device for GPU operations
         * @param model Whisper model data and parameters
         */
        DirectComputeEncoder(const DirectCompute::Device& device, const WhisperModel& model);

        /**
         * @brief Virtual destructor
         */
        virtual ~DirectComputeEncoder() = default;

        // iWhisperEncoder interface implementation

        /**
         * @brief Complete transcription without progress callbacks
         *
         * Performs full encode+decode cycle using DirectCompute pipeline.
         * This method maintains compatibility with the original implementation.
         *
         * @param spectrogram Input MEL spectrogram data
         * @param resultSink Output pointer to store transcription results
         * @return HRESULT S_OK on success, error code on failure
         */
        virtual HRESULT encode(
            iSpectrogram& spectrogram,
            iTranscribeResult** resultSink
        ) override;

        /**
         * @brief Complete transcription with progress support
         *
         * Performs full encode+decode cycle with progress reporting.
         * Note: DirectCompute implementation has limited progress granularity.
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
        ) override;

        /**
         * @brief Encode-only operation for streaming pipeline
         *
         * Performs only the encoding phase using DirectCompute shaders.
         * Stores encoded state for later decoding.
         *
         * @param spectrogram Input MEL spectrogram data
         * @param seek Seek offset parameter for audio positioning
         * @return HRESULT S_OK on success, error code on failure
         */
        virtual HRESULT encodeOnly(
            iSpectrogram& spectrogram,
            int seek
        ) override;

        /**
         * @brief Decode-only operation for streaming pipeline
         * 
         * Performs decoding using previously encoded state.
         * Uses DirectCompute for token generation and probability calculation.
         * 
         * @param resultSink Output pointer to store transcription results
         * @return HRESULT S_OK on success, error code on failure
         */
        virtual HRESULT decodeOnly(
            iTranscribeResult** resultSink
        ) override;

        /**
         * @brief Get implementation information
         * 
         * @return const char* "DirectCompute" identifier
         */
        virtual const char* getImplementationName() const override;

        /**
         * @brief Check if the encoder is ready for operation
         *
         * Verifies DirectCompute device and context are properly initialized.
         *
         * @return bool true if ready, false if not initialized
         */
        virtual bool isReady() const override;

        /**
         * @brief Check if the encoder supports PCM direct input
         *
         * DirectCompute implementation does not support PCM direct input,
         * as it requires MEL spectrogram data for GPU processing.
         *
         * @return bool Always returns false for DirectCompute implementation
         */
        virtual bool supportsPcmInput() const override;

        /**
         * @brief Direct PCM transcription method (not supported)
         *
         * DirectCompute implementation does not support PCM direct input.
         * This method always returns E_NOTIMPL.
         *
         * @param buffer Input audio buffer (ignored)
         * @param progress Progress callbacks (ignored)
         * @param resultSink Output pointer (ignored)
         * @return HRESULT Always returns E_NOTIMPL
         */
        virtual HRESULT transcribePcm(
            const iAudioBuffer* buffer,
            const sProgressSink& progress,
            iTranscribeResult** resultSink
        ) override;

    private:
        /**
         * @brief Internal encode implementation
         * 
         * Core encoding logic extracted from original ContextImpl::encode.
         * Handles DirectCompute shader execution and parameter setup.
         * 
         * @param mel Input spectrogram
         * @param seek Seek offset
         * @return HRESULT Result code
         */
        HRESULT internalEncode(iSpectrogram& mel, int seek);

        /**
         * @brief Internal decode implementation
         * 
         * Core decoding logic for token generation.
         * Handles probability calculation and result assembly.
         * 
         * @param tokens Input token sequence
         * @param length Token count
         * @param n_past Past token count
         * @param threads Thread count for processing
         * @return HRESULT Result code
         */
        HRESULT internalDecode(const int* tokens, size_t length, int n_past, int threads);

        /**
         * @brief Create encode parameters from model
         *
         * Sets up sEncodeParams structure from model parameters.
         * Maintains compatibility with original parameter handling.
         *
         * @param seek Seek offset
         * @return sEncodeParams Configured parameters
         */
        DirectCompute::sEncodeParams createEncodeParams(int seek) const;

        /**
         * @brief Create decode parameters from model
         *
         * Sets up sDecodeParams structure from model parameters.
         *
         * @param n_past Past token count
         * @return sDecodeParams Configured parameters
         */
        DirectCompute::sDecodeParams createDecodeParams(int n_past) const;


    };

} // namespace Whisper
