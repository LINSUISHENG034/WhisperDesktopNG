/*
 * WhisperCppEncoder.cpp - Adapter Implementation
 * Implements data conversion and result adaptation between new and old Whisper engines
 */

#include "stdafx.h"
#include "WhisperCppEncoder.h"
#include "Whisper/audioConstants.h"
#include <vector>
#include <memory>
#include <cstring>

using namespace Whisper;

// Constructor implementation
WhisperCppEncoder::WhisperCppEncoder(const std::string& modelPath)
    : m_config{}
{
    try {
        m_engine = std::make_unique<CWhisperEngine>(modelPath, m_config);
    }
    catch (const CWhisperError& e) {
        // Log error but don't throw exception, let caller handle error through encode method return value
        m_engine = nullptr;
    }
}

WhisperCppEncoder::WhisperCppEncoder(const std::string& modelPath, const TranscriptionConfig& config)
    : m_config(config)
{
    try {
        m_engine = std::make_unique<CWhisperEngine>(modelPath, m_config);
    }
    catch (const CWhisperError& e) {
        m_engine = nullptr;
    }
}

WhisperCppEncoder::~WhisperCppEncoder() = default;

// Data conversion: from iSpectrogram -> std::vector<float>
HRESULT WhisperCppEncoder::extractMelData(iSpectrogram& spectrogram, std::vector<float>& audioFeatures)
{
    // D.2 LOG: extractMelData入口
    printf("[DEBUG] WhisperCppEncoder::extractMelData ENTRY\n");
    fflush(stdout);

    try
    {
        // Get spectrogram length (number of time steps)
        const size_t melLength = spectrogram.getLength();
        printf("[DEBUG] WhisperCppEncoder::extractMelData: melLength=%zu\n", melLength);
        if (melLength == 0) {
            printf("[DEBUG] WhisperCppEncoder::extractMelData ERROR: melLength is 0\n");
            return E_INVALIDARG;
        }

        // Calculate total number of floats: time steps * MEL bands
        const size_t totalFloats = melLength * N_MEL;
        audioFeatures.resize(totalFloats);

        // Get complete MEL data from iSpectrogram
        const float* pBuffer = nullptr;
        size_t stride = 0;

        // Call makeBuffer to get complete spectrogram data
        HRESULT hr = spectrogram.makeBuffer(0, melLength, &pBuffer, stride);
        if (FAILED(hr)) {
            return hr; // If extraction fails, return error code directly
        }

        if (pBuffer == nullptr) {
            return E_POINTER;
        }

        // Understand data layout:
        // - iSpectrogram data layout: each time step contains N_MEL bands
        // - stride represents step size between each band (in float units)
        // - We need to rearrange data into continuous format for new engine

        if (stride == melLength) {
            // Data is stored band-first (each band is a continuous time series)
            // Need to transpose to time-first format
            for (size_t t = 0; t < melLength; t++) {
                for (size_t mel = 0; mel < N_MEL; mel++) {
                    audioFeatures[t * N_MEL + mel] = pBuffer[mel * stride + t];
                }
            }
        }
        else {
            // Assume data is already in time-first continuous format
            std::memcpy(audioFeatures.data(), pBuffer, totalFloats * sizeof(float));
        }

        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_UNEXPECTED;
    }
}

// Time conversion: from milliseconds to 100-nanosecond ticks
uint64_t WhisperCppEncoder::millisecondsToTicks(int64_t milliseconds) const
{
    // 1 millisecond = 10,000 100-nanosecond ticks
    return static_cast<uint64_t>(milliseconds * 10000);
}

// Get engine information
std::string WhisperCppEncoder::getModelType() const
{
    if (m_engine) {
        return m_engine->getModelType();
    }
    return "Unknown";
}

bool WhisperCppEncoder::isMultilingual() const
{
    if (m_engine) {
        return m_engine->isMultilingual();
    }
    return false;
}

// String management: ensure strings remain valid during result object lifetime
void WhisperCppEncoder::manageResultStrings(TranscribeResult& result, const TranscriptionResult& engineResult)
{
    // Clear previous string storage
    result.segmentsText.clear();
    result.segmentsText.reserve(engineResult.segments.size());

    // Copy all segment text to internal storage
    for (const auto& segment : engineResult.segments) {
        result.segmentsText.push_back(segment.text);
    }
}

// Result conversion: from TranscriptionResult -> TranscribeResult
HRESULT WhisperCppEncoder::convertResults(const TranscriptionResult& engineResult, TranscribeResult& result)
{
    try
    {
        if (!engineResult.success) {
            return E_FAIL;
        }

        // Clear previous results
        result.segments.clear();
        result.tokens.clear();

        // First manage string storage
        manageResultStrings(result, engineResult);

        // Convert segments
        result.segments.reserve(engineResult.segments.size());
        for (size_t i = 0; i < engineResult.segments.size(); i++) {
            const auto& srcSegment = engineResult.segments[i];
            sSegment destSegment = {};

            // Set text pointer (pointing to our internal string storage)
            destSegment.text = result.segmentsText[i].c_str();

            // Convert time (from milliseconds to 100-nanosecond ticks)
            destSegment.time.begin.ticks = millisecondsToTicks(srcSegment.startTime);
            destSegment.time.end.ticks = millisecondsToTicks(srcSegment.endTime);

            // Set token range (currently new engine doesn't provide token-level information)
            destSegment.firstToken = 0;
            destSegment.countTokens = 0;

            result.segments.push_back(destSegment);
        }

        // Note: The new CWhisperEngine currently doesn't provide token-level detailed information
        // If token information is needed, the CWhisperEngine interface may need to be extended
        // For now we only provide segment-level results
        result.tokens.clear();

        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_UNEXPECTED;
    }
}

// Core adaptation/conversion logic
HRESULT WhisperCppEncoder::encode(iSpectrogram& spectrogram, iTranscribeResult** resultSink)
{
    // B.1 LOG: WhisperCppEncoder::encode入口
    printf("[DEBUG] WhisperCppEncoder::encode ENTRY\n");

    if (resultSink == nullptr) {
        printf("[DEBUG] WhisperCppEncoder::encode ERROR: resultSink is NULL\n");
        return E_POINTER;
    }

    if (m_engine == nullptr) {
        printf("[DEBUG] WhisperCppEncoder::encode ERROR: m_engine is NULL\n");
        return E_FAIL; // Engine initialization failed
    }

    try
    {
        // 1. Data conversion: from iSpectrogram -> std::vector<float>
        // This is the most critical step, need to extract complete MEL data from spectrogram
        std::vector<float> audioFeatures;
        HRESULT hr = extractMelData(spectrogram, audioFeatures);
        if (FAILED(hr)) {
            printf("[DEBUG] WhisperCppEncoder::encode ERROR: extractMelData failed, hr=0x%08X\n", hr);
            return hr;
        }

        // B.1 LOG: 打印提取的MEL数据大小
        printf("[DEBUG] WhisperCppEncoder::encode: extracted audioFeatures.size()=%zu\n", audioFeatures.size());

        // B.1 LOG: 打印即将调用的方法
        printf("[DEBUG] WhisperCppEncoder::encode: About to call m_engine->transcribe(audioFeatures, m_config)\n");
        fflush(stdout);

        // 2. Call new engine's core transcription method with empty progress sink
        TranscriptionResult engineResult = m_engine->transcribe(audioFeatures, m_config, Whisper::sProgressSink{});

        // B.1 LOG: 打印引擎返回的结果
        printf("[DEBUG] WhisperCppEncoder::encode: engine returned success=%s, segments.size()=%zu\n", engineResult.success ? "true" : "false", engineResult.segments.size());

        // 3. Create COM object for result
        ComLight::CComPtr<ComLight::Object<TranscribeResult>> resultObj;
        hr = ComLight::Object<TranscribeResult>::create(resultObj);
        if (FAILED(hr)) {
            printf("[DEBUG] WhisperCppEncoder::encode ERROR: failed to create result object, hr=0x%08X\n", hr);
            return hr;
        }

        // 4. Result conversion: from TranscriptionResult -> TranscribeResult
        hr = convertResults(engineResult, *resultObj);
        if (FAILED(hr)) {
            printf("[DEBUG] WhisperCppEncoder::encode ERROR: convertResults failed, hr=0x%08X\n", hr);
            return hr;
        }

        // B.1 LOG: 打印转换后的结果
        printf("[DEBUG] WhisperCppEncoder::encode: converted result segments.size()=%zu, tokens.size()=%zu\n",
               resultObj->segments.size(), resultObj->tokens.size());

        // 5. Return result interface
        resultObj.detach(resultSink);

        printf("[DEBUG] WhisperCppEncoder::encode EXIT: SUCCESS\n");
        return S_OK;
    }
    catch (const CWhisperError& e)
    {
        // Catch our engine's exceptions and convert to HRESULT error code
        // Can log error information e.what() here
        return E_FAIL;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        // Catch other unknown exceptions
        return E_UNEXPECTED;
    }
}

// Enhanced encode method with progress callback support
HRESULT WhisperCppEncoder::encode(iSpectrogram& spectrogram, const sProgressSink& progress, iTranscribeResult** resultSink)
{
    // B.1 LOG: WhisperCppEncoder::encode(progress)入口
    printf("[DEBUG] WhisperCppEncoder::encode(progress) ENTRY\n");

    if (resultSink == nullptr) {
        printf("[DEBUG] WhisperCppEncoder::encode(progress) ERROR: resultSink is NULL\n");
        return E_POINTER;
    }

    if (m_engine == nullptr) {
        printf("[DEBUG] WhisperCppEncoder::encode(progress) ERROR: m_engine is NULL\n");
        return E_FAIL; // Engine initialization failed
    }

    try
    {
        // 1. Data conversion: from iSpectrogram -> std::vector<float>
        std::vector<float> audioFeatures;
        HRESULT hr = extractMelData(spectrogram, audioFeatures);
        if (FAILED(hr)) {
            printf("[DEBUG] WhisperCppEncoder::encode(progress) ERROR: extractMelData failed, hr=0x%08X\n", hr);
            return hr;
        }

        // B.1 LOG: 打印提取的MEL数据大小
        printf("[DEBUG] WhisperCppEncoder::encode(progress): extracted audioFeatures.size()=%zu\n", audioFeatures.size());

        // B.1 LOG: 打印即将调用的方法
        printf("[DEBUG] WhisperCppEncoder::encode(progress): About to call m_engine->transcribe(audioFeatures, m_config, progress)\n");
        printf("[DEBUG] WhisperCppEncoder::encode(progress): m_engine=%p, audioFeatures.size()=%zu\n", m_engine.get(), audioFeatures.size());
        fflush(stdout);

        // 2. Call new engine's transcription method with progress callback
        // Pass the progress callback to the engine for progress reporting and cancellation support
        TranscriptionResult engineResult = m_engine->transcribe(audioFeatures, m_config, progress);

        // B.1 LOG: 打印调用后的结果
        printf("[DEBUG] WhisperCppEncoder::encode(progress): m_engine->transcribe returned, engineResult.success=%s, segments.size()=%zu\n",
               engineResult.success ? "true" : "false", engineResult.segments.size());
        fflush(stdout);

        // 3. Create COM object for result
        ComLight::CComPtr<ComLight::Object<TranscribeResult>> resultObj;
        hr = ComLight::Object<TranscribeResult>::create(resultObj);
        if (FAILED(hr)) {
            return hr;
        }

        // 4. Result conversion: from TranscriptionResult -> TranscribeResult
        hr = convertResults(engineResult, *resultObj);
        if (FAILED(hr)) {
            return hr;
        }

        // 5. Return result interface
        resultObj.detach(resultSink);

        return S_OK;
    }
    catch (const CWhisperError& e)
    {
        // Catch our engine's exceptions and convert to HRESULT error code
        return E_FAIL;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        // Catch other unknown exceptions
        return E_UNEXPECTED;
    }
}

// Encode-only method that matches ContextImpl::encode signature
HRESULT WhisperCppEncoder::encodeOnly(iSpectrogram& spectrogram, int seek)
{
    if (m_engine == nullptr) {
        return E_FAIL; // Engine initialization failed
    }

    try
    {
        // 1. Data conversion: from iSpectrogram -> std::vector<float>
        std::vector<float> audioFeatures;
        HRESULT hr = extractMelData(spectrogram, audioFeatures);
        if (FAILED(hr)) {
            return hr;
        }

        // 2. Call the new CWhisperEngine::encode() method for true encode-only functionality
        // This will perform only the encoding phase and store the encoded state
        bool success = m_engine->encode(audioFeatures);
        if (!success) {
            return E_FAIL;
        }

        return S_OK;
    }
    catch (const CWhisperError& e)
    {
        return E_FAIL;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_UNEXPECTED;
    }
}

// Decode-only method that performs decoding after encoding
HRESULT WhisperCppEncoder::decodeOnly(iTranscribeResult** resultSink)
{
    if (resultSink == nullptr) {
        return E_POINTER;
    }

    if (m_engine == nullptr) {
        return E_FAIL; // Engine initialization failed
    }

    try
    {
        // 1. Call the new engine's decode method using previously encoded state
        TranscriptionResult engineResult = m_engine->decode(m_config);

        // 2. Create COM object for result
        ComLight::CComPtr<ComLight::Object<TranscribeResult>> resultObj;
        HRESULT hr = ComLight::Object<TranscribeResult>::create(resultObj);
        if (FAILED(hr)) {
            return hr;
        }

        // 3. Result conversion: from TranscriptionResult -> TranscribeResult
        hr = convertResults(engineResult, *resultObj);
        if (FAILED(hr)) {
            return hr;
        }

        // 4. Return result interface
        resultObj.detach(resultSink);

        return S_OK;
    }
    catch (const CWhisperError& e)
    {
        // Catch our engine's exceptions and convert to HRESULT error code
        // Can log error information e.what() here
        return E_FAIL;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        // Catch other unknown exceptions
        return E_UNEXPECTED;
    }
}

// Interface required methods implementation

const char* WhisperCppEncoder::getImplementationName() const
{
    return "WhisperCpp";
}

bool WhisperCppEncoder::isReady() const
{
    return m_engine != nullptr;
}
