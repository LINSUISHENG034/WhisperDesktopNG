#include "stdafx.h"
#include "modelFactory.h"
#include "API/iContext.cl.h"

HRESULT COMLIGHTCALL Whisper::loadModel( const wchar_t* path, const sModelSetup& setup, const sLoadModelCallbacks* callbacks, iModel** pp )
{
#if WHISPER_NG_USE_ONLY_CPP_IMPLEMENTATION
	// Simplified path: Always use GPU model (which now uses WhisperCppEncoder)
	printf("[DEBUG] loadModel: Using simplified path - always loading GPU model with WhisperCppEncoder\n");
	return loadGpuModel( path, setup, callbacks, pp );
#else
	// Original implementation with multiple engine support
	switch( setup.impl )
	{
	case eModelImplementation::GPU:
	case eModelImplementation::Hybrid:
		return loadGpuModel( path, setup, callbacks, pp );
	case eModelImplementation::Reference:
		if( 0 != setup.flags )
			logWarning( u8"The reference model doesn’t currently use any flags, argument ignored" );
		return loadReferenceCpuModel( path, pp );
	}

	logError( u8"Unknown model implementation 0x%X", (int)setup.impl );
	return E_INVALIDARG;
#endif
}