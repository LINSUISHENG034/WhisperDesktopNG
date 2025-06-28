// Q8_0 Dequantization Compute Shader
// Converts GGML Q8_0 quantized data to FP32 format on GPU
// Each thread group processes multiple Q8_0 blocks in parallel

#include "ggmlQuantization.hlsli"

// Input: ByteAddressBuffer containing Q8_0 quantized data
ByteAddressBuffer quantizedData : register(t0);

// Output: RWBuffer for dequantized FP32 data  
RWBuffer<float> dequantizedData : register(u0);

// Constants buffer with tensor metadata
cbuffer Constants : register(b0) {
    uint totalElements;     // Total number of elements to dequantize
    uint blockCount;        // Number of Q8_0 blocks
    uint outputOffset;      // Offset in output buffer (for tensor slicing)
    uint inputOffset;       // Offset in input buffer (for tensor slicing)
};

// Thread group size: 32 threads per group
// Each thread processes one element within a Q8_0 block
[numthreads(32, 1, 1)]
void main(uint3 groupId : SV_GroupID, uint threadId : SV_GroupIndex) {
    // Calculate which Q8_0 block this thread group is processing
    uint blockIndex = groupId.x;
    if (blockIndex >= blockCount) return;
    
    // Load the Q8_0 block from input buffer
    block_q8_0 block = load_q8_0_block(quantizedData, blockIndex + inputOffset);
    
    // Each thread in the group processes one element of the block
    uint elementIndex = threadId;  // 0-31
    
    // Calculate global output index
    uint globalIndex = blockIndex * QK8_0 + elementIndex + outputOffset;
    if (globalIndex >= totalElements) return;
    
    // Convert ggml_half to float
    const float scale = ggml_half_to_float(block.d);
    
    // GGML Q8_0 dequantization algorithm (matching CPU implementation)
    // Each qs byte is a signed 8-bit value (int8_t)
    
    // Extract the 8-bit value for this element
    uint byteIndex = elementIndex / 4;         // Which uint32 contains this element
    uint byteOffset = (elementIndex % 4) * 8;  // Byte offset within the uint32
    
    uint quantized_byte = extractBits(block.qs[byteIndex], byteOffset, 8);
    
    // Convert from unsigned to signed 8-bit value (int8_t)
    // In GGML, Q8_0 uses int8_t directly, so we need to interpret as signed
    int signed_val;
    if (quantized_byte >= 128) {
        signed_val = (int)quantized_byte - 256;  // Convert to negative
    } else {
        signed_val = (int)quantized_byte;        // Already positive
    }
    
    // Dequantize: value = scale * signed_val
    float dequantized_value = scale * (float)signed_val;
    
    // Store result in output buffer
    dequantizedData[globalIndex] = dequantized_value;
}
