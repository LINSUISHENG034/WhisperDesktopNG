#pragma once
#include "iSpectrogram.h"
#include "audioConstants.h"
#include <vector>
#include <string>
#include <memory>

// Forward declarations
struct whisper_context;

namespace Whisper
{
    // Forward declarations
    struct Filters;
    struct iAudioBuffer;

    // New iSpectrogram implementation using official whisper.cpp audio processing
    // This class replaces the problematic MelStreamer with a clean implementation
    // that uses whisper.cpp's official audio loading and mel conversion functions
    class WhisperCppSpectrogram : public iSpectrogram
    {
    private:
        // Mel spectrogram data storage
        std::vector<float> m_melData;
        size_t m_length = 0;  // Number of time steps (each = 10ms)
        
        // Audio data storage (for stereo PCM support)
        std::vector<float> m_audioData;
        bool m_hasStereoData = false;
        
        // Whisper context for mel conversion
        whisper_context* m_whisperCtx = nullptr;
        bool m_ownsContext = false;
        
        // Helper methods
        HRESULT loadAudioFromFile(const std::string& filePath);
        HRESULT convertPcmToMel(const std::vector<float>& pcmData, const Filters& filters, int threads = 1);
        void cleanup();
        
    public:
        // Constructor for file-based audio processing with filters
        WhisperCppSpectrogram(const std::string& audioFilePath, const Filters& filters);

        // Constructor for direct PCM data processing with filters
        WhisperCppSpectrogram(const std::vector<float>& pcmData, const Filters& filters, bool stereo = false);

        // Constructor for iAudioBuffer compatibility - this is the key for ContextImpl integration
        WhisperCppSpectrogram(const iAudioBuffer* buffer, const Filters& filters);

        // Constructor that mimics existing Spectrogram interface for maximum compatibility
        // This allows drop-in replacement of existing Spectrogram usage
        static std::unique_ptr<WhisperCppSpectrogram> createFromAudioFile(
            const std::string& audioFilePath, const Filters& filters);
        
        // Destructor
        virtual ~WhisperCppSpectrogram();
        
        // iSpectrogram interface implementation
        
        // Make a buffer with length * N_MEL floats, starting at the specified offset
        HRESULT makeBuffer(size_t offset, size_t length, const float** buffer, size_t& stride) noexcept override;
        
        // Get the length of the spectrogram (in 10ms units)
        size_t getLength() const noexcept override;
        
        // Copy stereo PCM data if available
        HRESULT copyStereoPcm(size_t offset, size_t length, std::vector<StereoSample>& buffer) const override;
        
        // Additional utility methods
        
        // Check if the spectrogram was successfully created
        bool isValid() const noexcept;
        
        // Get memory usage in bytes
        size_t getMemoryUsage() const noexcept;
        
        // Get audio duration in milliseconds
        double getDurationMs() const noexcept;
        
        // Disable copy constructor and assignment
        WhisperCppSpectrogram(const WhisperCppSpectrogram&) = delete;
        WhisperCppSpectrogram& operator=(const WhisperCppSpectrogram&) = delete;
    };
}
