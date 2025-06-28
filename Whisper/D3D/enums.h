#pragma once
#include <stdint.h>
#include <dxgi.h>
#include <assert.h>
// Include GGML quantization block definitions
#include "../source/ggml-quants.h"

namespace DirectCompute
{
	enum struct eDataType : uint8_t
	{
		FP16,
		FP32,
		U32,
		// GGML quantization types
		Q4_0,		// 4-bit quantization, 32 elements per block
		Q5_1,		// 5-bit quantization, 32 elements per block
		Q8_0,		// 8-bit quantization, 32 elements per block
	};

	inline size_t elementSize( eDataType dt )
	{
		switch( dt )
		{
			case eDataType::FP16:
				return 2;
			case eDataType::FP32:
			case eDataType::U32:
				return 4;
			case eDataType::Q4_0:
				return 18;  // 18 bytes per Q4_0 block (32 elements)
			case eDataType::Q5_1:
				return 24;  // 24 bytes per Q5_1 block (32 elements)
			case eDataType::Q8_0:
				return 34;  // 34 bytes per Q8_0 block (32 elements)
			default:
				assert( false );
				return 0;
		}
	}

	DXGI_FORMAT viewFormat( eDataType dt );

	enum struct eBufferUse : uint8_t
	{
		// Immutable tensor, readable from GPU
		Immutable,
		// Read+write tensor, readable and writable on GPU
		ReadWrite,
		// Read+write tensor, readable and writable on GPU, which supports downloads from GPU
		ReadWriteDownload,
		// The tensor is accessible by both GPU (read only) and CPU (write only). Optimized for resources frequently updated from CPU.
		Dynamic,
	};
}