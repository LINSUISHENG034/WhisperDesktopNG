#include "stdafx.h"
#include "WhisperCppSpectrogram.h"
#include "../AudioUtils/common-whisper.h"
#include "melSpectrogram.h"
#include "WhisperModel.h"
#include "../API/iMediaFoundation.cl.h"
#include <algorithm>
#include <cassert>

using namespace Whisper;

// Constructor for file-based audio processing with filters
WhisperCppSpectrogram::WhisperCppSpectrogram(const std::string& audioFilePath, const Filters& filters)
    : m_whisperCtx(nullptr), m_ownsContext(false)
{
    // Load audio from file using official whisper.cpp function
    HRESULT hr = loadAudioFromFile(audioFilePath);
    if (FAILED(hr))
    {
        cleanup();
        return;
    }

    // Convert PCM to mel spectrogram using existing mel conversion algorithm
    hr = convertPcmToMel(m_audioData, filters);
    if (FAILED(hr))
    {
        cleanup();
        return;
    }
}

// Constructor for direct PCM data processing with filters
WhisperCppSpectrogram::WhisperCppSpectrogram(const std::vector<float>& pcmData, const Filters& filters, bool stereo)
    : m_audioData(pcmData), m_hasStereoData(stereo), m_whisperCtx(nullptr), m_ownsContext(false)
{
    // Convert PCM to mel spectrogram using existing mel conversion algorithm
    HRESULT hr = convertPcmToMel(m_audioData, filters);
    if (FAILED(hr))
    {
        cleanup();
        return;
    }
}

// Constructor for iAudioBuffer compatibility - key for ContextImpl integration
WhisperCppSpectrogram::WhisperCppSpectrogram(const iAudioBuffer* buffer, const Filters& filters)
    : m_whisperCtx(nullptr), m_ownsContext(false)
{
    if (!buffer)
    {
        cleanup();
        return;
    }

    // Extract PCM data from iAudioBuffer
    const float* pcmMono = buffer->getPcmMono();
    const float* pcmStereo = buffer->getPcmStereo();
    uint32_t sampleCount = buffer->countSamples();

    if (!pcmMono || sampleCount == 0)
    {
        cleanup();
        return;
    }

    // Copy mono PCM data
    m_audioData.assign(pcmMono, pcmMono + sampleCount);

    // Check if stereo data is available
    m_hasStereoData = (pcmStereo != nullptr);

    // Convert PCM to mel spectrogram using existing mel conversion algorithm
    HRESULT hr = convertPcmToMel(m_audioData, filters);
    if (FAILED(hr))
    {
        cleanup();
        return;
    }
}

// Static factory method for maximum compatibility with existing code
std::unique_ptr<WhisperCppSpectrogram> WhisperCppSpectrogram::createFromAudioFile(
    const std::string& audioFilePath, const Filters& filters)
{
    return std::make_unique<WhisperCppSpectrogram>(audioFilePath, filters);
}

// Destructor
WhisperCppSpectrogram::~WhisperCppSpectrogram()
{
    cleanup();
}

// Load audio from file using official whisper.cpp function
HRESULT WhisperCppSpectrogram::loadAudioFromFile(const std::string& filePath)
{
    try
    {
        std::vector<std::vector<float>> pcmf32s;
        
        // Use official whisper.cpp audio loading function
        if (!read_audio_data(filePath, m_audioData, pcmf32s, false))
        {
            return E_FAIL;
        }
        
        // Check if we have stereo data
        if (pcmf32s.size() >= 2 && !pcmf32s[0].empty() && !pcmf32s[1].empty())
        {
            m_hasStereoData = true;
            // Store stereo data for copyStereoPcm method
            // Note: This is a simplified implementation
        }
        
        return S_OK;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

// Convert PCM to mel spectrogram using existing mel conversion algorithm
HRESULT WhisperCppSpectrogram::convertPcmToMel(const std::vector<float>& pcmData, const Filters& filters, int threads)
{
    if (pcmData.empty())
    {
        return E_FAIL;
    }

    try
    {
        // Calculate expected length based on audio duration
        m_length = pcmData.size() / FFT_STEP;
        if (m_length == 0)
        {
            return E_FAIL;
        }

        // Allocate mel data storage
        m_melData.resize(m_length * N_MEL);

        // Use existing mel conversion algorithm from the project
        // We need to get the filters from somewhere - for now use a default
        // TODO: Get proper filters from model or use default whisper filters

        // Create a simple mel context for conversion
        // This is a simplified approach - we'll use the existing SpectrogramContext
        // but we need access to filters

        // Use the provided filters with existing mel conversion algorithm
        SpectrogramContext melContext(filters);

        // Process audio in chunks
        for (size_t i = 0; i < m_length; i++)
        {
            const float* sourcePcm = pcmData.data() + i * FFT_STEP;
            size_t availableFloats = std::min((size_t)FFT_STEP, pcmData.size() - i * FFT_STEP);

            // Create output array for this chunk
            std::array<float, N_MEL> melChunk;

            // Convert this chunk to mel
            melContext.fft(melChunk, sourcePcm, availableFloats);

            // Copy to our storage
            // Data layout: time steps x mel bands (same as existing Spectrogram class)
            for (size_t j = 0; j < N_MEL; j++)
            {
                m_melData[i * N_MEL + j] = melChunk[j];
            }
        }

        return S_OK;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

// Cleanup resources
void WhisperCppSpectrogram::cleanup()
{
    // No whisper context to clean up in this implementation
    m_melData.clear();
    m_audioData.clear();
    m_length = 0;
}

// iSpectrogram interface implementation

HRESULT WhisperCppSpectrogram::makeBuffer(size_t offset, size_t length, const float** buffer, size_t& stride) noexcept
{
    if (!buffer)
    {
        return E_POINTER;
    }
    
    if (offset + length > m_length)
    {
        return E_BOUNDS;
    }
    
    if (m_melData.empty())
    {
        return E_FAIL;
    }
    
    // Return pointer to mel data at the specified offset
    // Data layout: time steps x mel bands, so offset is in time steps
    *buffer = &m_melData[offset * N_MEL];
    stride = m_length;  // Stride is the total length for this data layout
    
    return S_OK;
}

size_t WhisperCppSpectrogram::getLength() const noexcept
{
    return m_length;
}

HRESULT WhisperCppSpectrogram::copyStereoPcm(size_t offset, size_t length, std::vector<StereoSample>& buffer) const
{
    if (!m_hasStereoData)
    {
        return E_NOTIMPL;  // No stereo data available
    }
    
    // TODO: Implement stereo PCM copying
    // This would require storing the original stereo audio data
    return E_NOTIMPL;
}

// Utility methods

bool WhisperCppSpectrogram::isValid() const noexcept
{
    return !m_melData.empty() && m_length > 0;
}

size_t WhisperCppSpectrogram::getMemoryUsage() const noexcept
{
    return m_melData.size() * sizeof(float) + m_audioData.size() * sizeof(float);
}

double WhisperCppSpectrogram::getDurationMs() const noexcept
{
    return (double)m_length * 10.0;  // Each time step is 10ms
}
