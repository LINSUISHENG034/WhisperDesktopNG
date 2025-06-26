#pragma once
#include "iSpectrogram.h"
#include "audioConstants.h"
#include "MelStreamer.h"
#include <vector>
#include <string>
#include <memory>

namespace Whisper
{
    // Forward declarations
    struct Filters;
    struct iAudioReader;
    struct ProfileCollection;

    // Enhanced MelStreamer using official whisper.cpp audio processing
    // This class provides a drop-in replacement for MelStreamer with improved audio handling
    // while maintaining the same streaming interface and performance characteristics
    class WhisperCppMelStreamer : public MelStreamer
    {
    private:
        // Override the mel conversion to use whisper.cpp audio processing
        // This method replaces the problematic mel conversion with official whisper.cpp functions
        void convertChunkToMel(MelChunk& melChunk, const float* pcmData, size_t availableFloats);
        
    public:
        // Constructor that matches MelStreamer interface for drop-in replacement
        WhisperCppMelStreamer(const Filters& filters, ProfileCollection& profiler, const iAudioReader* reader);
        
        // Destructor
        virtual ~WhisperCppMelStreamer() = default;
        
        // Override makeBuffer to use improved mel conversion
        HRESULT makeBuffer(size_t offset, size_t length, const float** buffer, size_t& stride) noexcept override;
        
        // Disable copy constructor and assignment
        WhisperCppMelStreamer(const WhisperCppMelStreamer&) = delete;
        WhisperCppMelStreamer& operator=(const WhisperCppMelStreamer&) = delete;
    };
    
    // Single-threaded version using improved audio processing
    class WhisperCppMelStreamerSimple : public WhisperCppMelStreamer
    {
    public:
        WhisperCppMelStreamerSimple(const Filters& filters, ProfileCollection& profiler, const iAudioReader* reader) :
            WhisperCppMelStreamer(filters, profiler, reader) { }
    };
    
    // Multi-threaded version using improved audio processing
    class WhisperCppMelStreamerThread : public WhisperCppMelStreamer
    {
    private:
        // Additional threading support would be implemented here
        // For now, we inherit the base threading from MelStreamer
        
    public:
        WhisperCppMelStreamerThread(const Filters& filters, ProfileCollection& profiler, const iAudioReader* reader, int countThreads) :
            WhisperCppMelStreamer(filters, profiler, reader) 
        {
            // TODO: Implement multi-threading support
            // For now, this acts as a single-threaded version
        }
    };
}
