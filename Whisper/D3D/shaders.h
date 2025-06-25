#pragma once

#if !WHISPER_NG_USE_ONLY_CPP_IMPLEMENTATION
#include "shaderNames.h"

namespace DirectCompute
{
	HRESULT createComputeShaders( std::vector<CComPtr<ID3D11ComputeShader>>& shaders );

	void bindShader( eComputeShader shader );
}

#endif // !WHISPER_NG_USE_ONLY_CPP_IMPLEMENTATION