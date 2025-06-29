#include "stdafx.h"
#include "QuantizationOps.h"
#include "../D3D/shaders.h"
#include "../D3D/Binder.h"
#include "Device.h"
#include "mlUtils.h"

using namespace DirectCompute;

HRESULT QuantizationOps::dequantize( const Tensor& quantizedInput, Tensor& fp32Output, eDataType quantType )
{
	// Validate that the quantization type is supported
	if( !isQuantizedType( quantType ) )
	{
		return E_INVALIDARG;
	}

	// Dynamic dispatch based on quantization type
	switch( quantType )
	{
	case eDataType::Q4_0:
		return dequantizeQ4_0( quantizedInput, fp32Output );
	case eDataType::Q5_1:
		return dequantizeQ5_1( quantizedInput, fp32Output );
	case eDataType::Q8_0:
		return dequantizeQ8_0( quantizedInput, fp32Output );
	default:
		// This should never happen due to isQuantizedType check above
		return E_NOTIMPL;
	}
}

HRESULT QuantizationOps::dequantize( const Tensor& quantizedInput, Tensor& fp32Output )
{
	// Legacy function for backward compatibility
	// Attempt to determine quantization type from tensor size heuristics
	const uint32_t elementCount = fp32Output.countElements();
	const uint32_t inputSizeBytes = quantizedInput.countElements() * 4; // Assuming R32_UINT format

	// Calculate expected block count for each quantization type
	const uint32_t elementsPerBlock = 32;
	const uint32_t blockCount = (elementCount + elementsPerBlock - 1) / elementsPerBlock;

	// Check which quantization type matches the input size
	if( inputSizeBytes == blockCount * sizeof(block_q4_0) )
	{
		return dequantizeQ4_0( quantizedInput, fp32Output );
	}
	else if( inputSizeBytes == blockCount * sizeof(block_q5_1) )
	{
		return dequantizeQ5_1( quantizedInput, fp32Output );
	}
	else if( inputSizeBytes == blockCount * sizeof(block_q8_0) )
	{
		return dequantizeQ8_0( quantizedInput, fp32Output );
	}
	else
	{
		// Default to Q5_1 if size heuristics fail
		// This maintains backward compatibility with existing code
		return dequantizeQ5_1( quantizedInput, fp32Output );
	}
}

uint32_t QuantizationOps::getBlockCount( eDataType quantType, uint32_t elementCount )
{
	return calculateBlockCount( quantType, elementCount );
}

uint32_t QuantizationOps::getBlockSize( eDataType quantType )
{
	return getQuantizedBlockSize( quantType );
}

bool QuantizationOps::isQuantizedType( eDataType dt )
{
	return dt == eDataType::Q4_0 || dt == eDataType::Q5_1 || dt == eDataType::Q8_0;
}

HRESULT QuantizationOps::dequantizeQ4_0( const Tensor& input, Tensor& output )
{
	const uint32_t elementCount = output.countElements();
	const uint32_t blockCount = getBlockCount( eDataType::Q4_0, elementCount );
	
	return dispatchDequantizeShader( 
		eComputeShader::dequantizeQ4_0, 
		input, 
		output, 
		blockCount, 
		elementCount 
	);
}

HRESULT QuantizationOps::dequantizeQ5_1( const Tensor& input, Tensor& output )
{
	const uint32_t elementCount = output.countElements();
	const uint32_t blockCount = getBlockCount( eDataType::Q5_1, elementCount );
	
	return dispatchDequantizeShader( 
		eComputeShader::dequantizeQ5_1, 
		input, 
		output, 
		blockCount, 
		elementCount 
	);
}

HRESULT QuantizationOps::dequantizeQ8_0( const Tensor& input, Tensor& output )
{
	const uint32_t elementCount = output.countElements();
	const uint32_t blockCount = getBlockCount( eDataType::Q8_0, elementCount );
	
	return dispatchDequantizeShader( 
		eComputeShader::dequantizeQ8_0, 
		input, 
		output, 
		blockCount, 
		elementCount 
	);
}

HRESULT QuantizationOps::dispatchDequantizeShader( 
	eComputeShader shader,
	const Tensor& input, 
	Tensor& output,
	uint32_t blockCount,
	uint32_t totalElements )
{
	// Bind the compute shader
	bindShader( shader );

	// Set up constant buffer with parameters
	struct DequantizeConstants
	{
		uint32_t totalElements;
		uint32_t blockCount;
		uint32_t outputOffset;
		uint32_t inputOffset;
	};

	DequantizeConstants constants = {
		totalElements,
		blockCount,
		0,  // outputOffset - for now, always 0
		0   // inputOffset - for now, always 0
	};

	// Update constant buffer
	const __m128i cbData = load16( (const uint32_t*)&constants );
	ID3D11Buffer* cb = updateSmallCb( cbData );

	// Bind resources
	Binder binder;
	binder.bind( (ID3D11ShaderResourceView*)input, (ID3D11UnorderedAccessView*)output );

	// Set constant buffer
	ID3D11DeviceContext* ctx = context();
	ctx->CSSetConstantBuffers( 0, 1, &cb );

	// Dispatch compute shader
	// Each thread group processes one quantization block (32 elements)
	// So we need blockCount thread groups
	ctx->Dispatch( blockCount, 1, 1 );

	return S_OK;
}
