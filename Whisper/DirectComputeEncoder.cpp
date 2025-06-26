/*
 * DirectComputeEncoder.cpp - DirectCompute Implementation Wrapper
 * 
 * Implementation of DirectComputeEncoder class that wraps the original
 * DirectCompute-based Whisper implementation to conform to iWhisperEncoder interface.
 */

#include "stdafx.h"
#include "DirectComputeEncoder.h"
#include "Whisper/audioConstants.h"
#include "Whisper/sEncodeParams.h"
#include "API/TranscribeStructs.h"
#include <stdexcept>

using namespace Whisper;

// Constructor
DirectComputeEncoder::DirectComputeEncoder(const DirectCompute::Device& device, const WhisperModel& model)
    : m_device(device)
    , m_model(model)
    , m_context(model, m_profiler)
    , m_profiler(model)
    , m_isEncoded(false)
    , m_lastSeek(0)
{
    // Initialize probability buffer
    // Size based on model vocabulary size
    m_probs.resize(model.parameters.n_vocab);
}

// Interface implementation methods

HRESULT DirectComputeEncoder::encode(
    iSpectrogram& spectrogram,
    iTranscribeResult** resultSink)
{
    if (!resultSink) {
        return E_POINTER;
    }

    // For DirectCompute implementation, we perform complete transcription
    // This is a simplified implementation that calls the full pipeline
    // TODO: Implement full transcription logic similar to ContextImpl::runFullImpl

    // For now, delegate to encode-only + decode-only pattern
    HRESULT hr = encodeOnly(spectrogram, 0);
    if (FAILED(hr)) {
        return hr;
    }

    return decodeOnly(resultSink);
}

HRESULT DirectComputeEncoder::encode(
    iSpectrogram& spectrogram,
    const sProgressSink& progress,
    iTranscribeResult** resultSink)
{
    if (!resultSink) {
        return E_POINTER;
    }

    // DirectCompute implementation has limited progress reporting granularity
    // We can call progress callbacks at major milestones using the pfn callback

    if (progress.pfn) {
        HRESULT hr = progress.pfn(0.0, nullptr, progress.pv); // Starting
        if (FAILED(hr)) {
            return hr; // User requested cancellation
        }
    }

    HRESULT hr = encodeOnly(spectrogram, 0);
    if (FAILED(hr)) {
        return hr;
    }

    if (progress.pfn) {
        hr = progress.pfn(0.5, nullptr, progress.pv); // Encoding complete
        if (FAILED(hr)) {
            return hr; // User requested cancellation
        }
    }

    hr = decodeOnly(resultSink);
    if (FAILED(hr)) {
        return hr;
    }

    if (progress.pfn) {
        progress.pfn(1.0, nullptr, progress.pv); // Complete
    }

    return S_OK;
}

HRESULT DirectComputeEncoder::encodeOnly(
    iSpectrogram& spectrogram,
    int seek)
{
    auto prof = m_profiler.cpuBlock(eCpuBlock::Encode);

    try {
        HRESULT hr = internalEncode(spectrogram, seek);
        if (SUCCEEDED(hr)) {
            m_isEncoded = true;
            m_lastSeek = seek;
        }
        return hr;
    }
    catch (HRESULT hr) {
        return hr;
    }
    catch (...) {
        return E_UNEXPECTED;
    }
}

HRESULT DirectComputeEncoder::decodeOnly(
    iTranscribeResult** resultSink)
{
    if (!resultSink) {
        return E_POINTER;
    }

    if (!m_isEncoded) {
        return E_FAIL; // Must call encodeOnly first
    }

    // TODO: Implement full decoding logic
    // For now, create an empty result as placeholder
    ComLight::CComPtr<ComLight::Object<TranscribeResult>> resultObj;
    HRESULT hr = ComLight::Object<TranscribeResult>::create(resultObj);
    if (FAILED(hr)) {
        return hr;
    }

    // Initialize with empty result
    // TODO: Implement actual decoding and result population
    
    resultObj.detach(resultSink);
    return S_OK;
}

const char* DirectComputeEncoder::getImplementationName() const
{
    return "DirectCompute";
}

bool DirectComputeEncoder::isReady() const
{
    // Check if DirectCompute device and context are properly initialized
    // This is a basic check - in a full implementation, we might verify
    // GPU device state, shader compilation, etc.
    return true; // Assume ready if constructor succeeded
}

// PCM direct input support implementation
bool DirectComputeEncoder::supportsPcmInput() const
{
    // DirectCompute implementation does not support PCM direct input
    // It requires MEL spectrogram data for GPU processing
    return false;
}

// PCM direct transcription implementation (not supported)
HRESULT DirectComputeEncoder::transcribePcm(
    const iAudioBuffer* buffer,
    const sProgressSink& progress,
    iTranscribeResult** resultSink)
{
    // DirectCompute implementation does not support PCM direct input
    // Always return E_NOTIMPL to indicate this feature is not implemented
    return E_NOTIMPL;
}

// Private implementation methods

HRESULT DirectComputeEncoder::internalEncode(iSpectrogram& mel, int seek)
{
    // This is the core encoding logic extracted from ContextImpl::encode
    using namespace DirectCompute;

    DirectCompute::sEncodeParams ep = createEncodeParams(seek);

    try {
        auto cur = m_context.encode(mel, ep);
        // TODO: Store encoded tensor for later decoding
        return S_OK;
    }
    catch (HRESULT hr) {
        return hr;
    }
}

HRESULT DirectComputeEncoder::internalDecode(const int* tokens, size_t length, int n_past, int threads)
{
    // Core decoding logic extracted from ContextImpl::decode
    using namespace DirectCompute;
    
    DirectCompute::sDecodeParams dp = createDecodeParams(n_past);

    try {
        m_context.decode(tokens, (int)length, dp, m_probs, threads);
        return S_OK;
    }
    catch (HRESULT hr) {
        return hr;
    }
}

DirectCompute::sEncodeParams DirectComputeEncoder::createEncodeParams(int seek) const
{
    DirectCompute::sEncodeParams ep;
    ep.n_ctx = m_model.parameters.n_audio_ctx;
    ep.n_mels = m_model.parameters.n_mels;
    ep.mel_offset = seek;
    ep.layersCount = m_model.parameters.n_audio_layer;
    ep.n_state = m_model.parameters.n_audio_state;
    ep.n_head = m_model.parameters.n_audio_head;
    ep.n_audio_ctx = m_model.parameters.n_audio_ctx;
    ep.n_text_state = m_model.parameters.n_text_state;
    ep.n_text_layer = m_model.parameters.n_text_layer;
    ep.n_text_ctx = m_model.parameters.n_text_ctx;
    return ep;
}

DirectCompute::sDecodeParams DirectComputeEncoder::createDecodeParams(int n_past) const
{
    DirectCompute::sDecodeParams dp;
    dp.n_state = m_model.parameters.n_audio_state;
    dp.n_head = m_model.parameters.n_audio_head;
    dp.n_ctx = m_model.parameters.n_text_ctx;
    dp.n_past = n_past;
    dp.M = m_model.parameters.n_audio_ctx;
    dp.n_text_layer = m_model.parameters.n_text_layer;
    dp.n_vocab = m_model.parameters.n_vocab;
    return dp;
}


