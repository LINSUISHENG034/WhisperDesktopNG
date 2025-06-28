// Q4_0 Dequantization Compute Shader
// Converts GGML Q4_0 quantized data to FP32 format on GPU
// Each thread group processes multiple Q4_0 blocks in parallel

#include "ggmlQuantization.hlsli"

// Input: ByteAddressBuffer containing Q4_0 quantized data
ByteAddressBuffer quantizedData : register(t0);

// Output: RWBuffer for dequantized FP32 data  
RWBuffer<float> dequantizedData : register(u0);

// Constants buffer with tensor metadata
cbuffer Constants : register(b0) {
    uint totalElements;     // Total number of elements to dequantize
    uint blockCount;        // Number of Q4_0 blocks
    uint outputOffset;      // Offset in output buffer (for tensor slicing)
    uint inputOffset;       // Offset in input buffer (for tensor slicing)
};

// Thread group size: 32 threads per group
// Each thread processes one element within a Q4_0 block
[numthreads(32, 1, 1)]
void main(uint3 groupId : SV_GroupID, uint threadId : SV_GroupIndex) {
    // Calculate which Q4_0 block this thread group is processing
    uint blockIndex = groupId.x;
    if (blockIndex >= blockCount) return;
    
    // Load the Q4_0 block from input buffer
    block_q4_0 block = load_q4_0_block(quantizedData, blockIndex + inputOffset);
    
    // Each thread in the group processes one element of the block
    uint elementIndex = threadId;  // 0-31
    
    // Calculate global output index
    uint globalIndex = blockIndex * QK4_0 + elementIndex + outputOffset;
    if (globalIndex >= totalElements) return;
    
    // Convert ggml_half to float
    const float scale = ggml_half_to_float(block.d);
    
    // GGML Q4_0 dequantization algorithm (matching CPU implementation)
    // Each qs byte contains two 4-bit values
    
    uint j = elementIndex / 2;  // Which byte in qs array (0-15)
    uint is_upper = elementIndex % 2;  // 0 for lower nibble, 1 for upper nibble
    
    // Extract the byte containing our 4-bit value
    uint qs_byte = extractBits(block.qs[j / 4], (j % 4) * 8, 8);
    
    uint quantized_val;
    
    if (is_upper == 0) {
        // Lower nibble: elements 0-15 (j + 0)
        quantized_val = qs_byte & 0x0F;
    } else {
        // Upper nibble: elements 16-31 (j + 16)
        quantized_val = qs_byte >> 4;
    }
    
    // Convert from unsigned to signed 4-bit value (subtract 8)
    int signed_val = (int)quantized_val - 8;
    
    // Dequantize: value = scale * signed_val
    float dequantized_value = scale * (float)signed_val;
    
    // Store result in output buffer
    dequantizedData[globalIndex] = dequantized_value;
}
