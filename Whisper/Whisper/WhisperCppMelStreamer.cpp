#include "stdafx.h"
#include "WhisperCppMelStreamer.h"
#include "../AudioUtils/common-whisper.h"
#include "melSpectrogram.h"
#include <algorithm>
#include <cassert>

using namespace Whisper;

// Constructor that matches MelStreamer interface for drop-in replacement
WhisperCppMelStreamer::WhisperCppMelStreamer(const Filters& filters, ProfileCollection& profiler, const iAudioReader* reader)
    : MelStreamer(filters, profiler, reader)
{
    // Base class constructor handles all the initialization
    // We only override the mel conversion behavior
}

// Convert PCM chunk to mel using improved algorithm
void WhisperCppMelStreamer::convertChunkToMel(MelChunk& melChunk, const float* pcmData, size_t availableFloats)
{
    // For now, use the existing mel conversion algorithm from the base class
    // In a full implementation, this would use whisper.cpp's mel conversion
    // but that requires access to whisper context which is not available in streaming mode
    
    // Use the existing SpectrogramContext from the base class
    melContext.fft(melChunk, pcmData, availableFloats);
    
    // TODO: Replace with whisper.cpp mel conversion when context is available
    // This would require architectural changes to pass whisper context to streamers
}

// Override makeBuffer to use improved mel conversion
HRESULT WhisperCppMelStreamer::makeBuffer(size_t offset, size_t length, const float** buffer, size_t& stride) noexcept
{
    // For now, delegate to the base class implementation
    // The base class already handles all the complex streaming logic
    // We only need to override the mel conversion part
    
    // Check bounds
    if (offset < streamStartOffset)
    {
        logError(u8"WhisperCppMelStreamer doesn't support backwards seeks");
        return E_UNEXPECTED;
    }

    if (offset > streamStartOffset)
    {
        // The model wants to advance forward, drop now irrelevant chunks of data
        dropOldChunks(offset);
    }

    // Ensure we have enough mel chunks
    const size_t availableMel = queueMel.size();
    if (availableMel < length)
    {
        HRESULT hr = ensurePcmChunks(length);
        if (FAILED(hr))
            return hr;

        const size_t pcmChunks = serializePcm(availableMel);
        const size_t missingMelChunks = length - availableMel;
        const size_t loop1 = std::min(missingMelChunks, pcmChunks);
        
        {
            auto profilerBlock = profiler.cpuBlock(eCpuBlock::Spectrogram);
            for (size_t i = 0; i < loop1; i++)
            {
                auto& melChunk = queueMel.emplace_back();
                const float* sourcePcm = tempPcm.data() + i * FFT_STEP;
                size_t availableChunks = pcmChunks - i;
                size_t availableFloats = availableChunks * FFT_STEP;
                
                // Use our improved mel conversion
                convertChunkToMel(melChunk, sourcePcm, availableFloats);
            }
        }

        // Handle case where we don't have enough PCM data
        if (loop1 < missingMelChunks)
        {
            // Fill remaining chunks with zeros
            for (size_t i = loop1; i < missingMelChunks; i++)
            {
                auto& melChunk = queueMel.emplace_back();
                std::fill(melChunk.begin(), melChunk.end(), 0.0f);
            }
        }
    }

    // Serialize mel data to output buffer
    const size_t totalMelChunks = queueMel.size();
    if (totalMelChunks < length)
    {
        return E_FAIL; // Not enough data available
    }

    // Resize output buffer if needed
    outputMel.resize(length * N_MEL);

    // Copy mel data to output buffer
    for (size_t i = 0; i < length; i++)
    {
        const auto& melChunk = queueMel[i];
        for (size_t j = 0; j < N_MEL; j++)
        {
            outputMel[i * N_MEL + j] = melChunk[j];
        }
    }

    // Apply normalization (copied from base class)
    normalizeOutput();

    // Return buffer pointer
    *buffer = outputMel.data();
    stride = length;

    return S_OK;
}
