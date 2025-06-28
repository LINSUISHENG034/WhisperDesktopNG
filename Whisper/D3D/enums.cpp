#include "stdafx.h"
#include "enums.h"

static const alignas( 16 ) std::array<DXGI_FORMAT, 6> s_tensorViewFormats = {
	DXGI_FORMAT_R16_FLOAT,		// FP16
	DXGI_FORMAT_R32_FLOAT,		// FP32
	DXGI_FORMAT_R32_UINT,		// U32
	DXGI_FORMAT_R8_UINT,		// Q4_0 - use byte format for quantized data
	DXGI_FORMAT_R8_UINT,		// Q5_1 - use byte format for quantized data
	DXGI_FORMAT_R8_UINT,		// Q8_0 - use byte format for quantized data
};

DXGI_FORMAT DirectCompute::viewFormat( eDataType dt )
{
	const uint8_t index = (uint8_t)dt;
	assert( index < s_tensorViewFormats.size() );
	return s_tensorViewFormats[ index ];
}