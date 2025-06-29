#pragma once
#include "../D3D/enums.h"
#include "../D3D/shaderNames.h"
#include "Tensor.h"

namespace DirectCompute
{
	// GGML quantization block structures (matching HLSL definitions)
	struct block_q4_0 {
		uint16_t d;         // scale (ggml_half as uint16)
		uint8_t qs[16];     // quantized values (16 bytes)
	};

	struct block_q5_1 {
		uint16_t d;         // scale (ggml_half as uint16)
		uint16_t m;         // min (ggml_half as uint16)
		uint32_t qh;        // high bits (4 bytes)
		uint8_t qs[16];     // quantized values (16 bytes)
	};

	struct block_q8_0 {
		uint16_t d;         // scale (ggml_half as uint16)
		uint8_t qs[32];     // quantized values (32 bytes)
	};

	// Quantization operations class
	class QuantizationOps
	{
	public:
		// Dequantize a quantized tensor to FP32 format
		// Input: quantized tensor with Q4_0, Q5_1, or Q8_0 data
		// Output: FP32 tensor with dequantized values
		// quantType: explicit quantization type (Q4_0, Q5_1, or Q8_0)
		static HRESULT dequantize( const Tensor& quantizedInput, Tensor& fp32Output, eDataType quantType );

		// Legacy dequantize function for backward compatibility
		// Attempts to determine quantization type from tensor size heuristics
		static HRESULT dequantize( const Tensor& quantizedInput, Tensor& fp32Output );

		// Get the number of blocks for a given quantization type and element count
		static uint32_t getBlockCount( eDataType quantType, uint32_t elementCount );

		// Get the block size in bytes for a quantization type
		static uint32_t getBlockSize( eDataType quantType );

		// Check if a data type is a quantized type
		static bool isQuantizedType( eDataType dt );

	private:
		// Internal dispatch methods for each quantization type
		static HRESULT dequantizeQ4_0( const Tensor& input, Tensor& output );
		static HRESULT dequantizeQ5_1( const Tensor& input, Tensor& output );
		static HRESULT dequantizeQ8_0( const Tensor& input, Tensor& output );

		// Helper method to dispatch compute shader
		static HRESULT dispatchDequantizeShader(
			eComputeShader shader,
			const Tensor& input,
			Tensor& output,
			uint32_t blockCount,
			uint32_t totalElements
		);
	};

	// Utility functions for quantization block size calculations
	inline uint32_t getQuantizedBlockSize( eDataType dt )
	{
		switch( dt )
		{
		case eDataType::Q4_0:
			return sizeof(block_q4_0);  // 18 bytes
		case eDataType::Q5_1:
			return sizeof(block_q5_1);  // 24 bytes
		case eDataType::Q8_0:
			return sizeof(block_q8_0);  // 34 bytes
		default:
			return 0;
		}
	}

	inline uint32_t getQuantizedElementsPerBlock( eDataType dt )
	{
		switch( dt )
		{
		case eDataType::Q4_0:
		case eDataType::Q5_1:
		case eDataType::Q8_0:
			return 32;  // All GGML quantization types use 32 elements per block
		default:
			return 0;
		}
	}

	inline uint32_t calculateBlockCount( eDataType dt, uint32_t elementCount )
	{
		const uint32_t elementsPerBlock = getQuantizedElementsPerBlock( dt );
		if( elementsPerBlock == 0 )
			return 0;
		return ( elementCount + elementsPerBlock - 1 ) / elementsPerBlock;
	}
}
