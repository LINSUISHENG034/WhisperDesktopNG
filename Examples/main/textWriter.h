#pragma once
#include "../../Whisper/API/iContext.cl.h"

// These functions print output segments into text files of various formats
HRESULT writeText( Whisper::iContext* context, LPCTSTR audioPath, bool timestamps, LPCTSTR pattern = nullptr, LPCTSTR modelPath = nullptr, LPCTSTR gpu = nullptr );
HRESULT writeSubRip( Whisper::iContext* context, LPCTSTR audioPath, LPCTSTR pattern = nullptr, LPCTSTR modelPath = nullptr, LPCTSTR gpu = nullptr );
HRESULT writeWebVTT( Whisper::iContext* context, LPCTSTR audioPath, LPCTSTR pattern = nullptr, LPCTSTR modelPath = nullptr, LPCTSTR gpu = nullptr );