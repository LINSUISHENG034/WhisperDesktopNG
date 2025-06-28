// Q5_1 Dequantization Compute Shader
// Converts GGML Q5_1 quantized data to FP32 format on GPU
// Each thread group processes multiple Q5_1 blocks in parallel

#include "ggmlQuantization.hlsli"

// Input: ByteAddressBuffer containing Q5_1 quantized data
ByteAddressBuffer quantizedData : register(t0);

// Output: RWBuffer for dequantized FP32 data  
RWBuffer<float> dequantizedData : register(u0);

// Constants buffer with tensor metadata
cbuffer Constants : register(b0) {
    uint totalElements;     // Total number of elements to dequantize
    uint blockCount;        // Number of Q5_1 blocks
    uint outputOffset;      // Offset in output buffer (for tensor slicing)
    uint inputOffset;       // Offset in input buffer (for tensor slicing)
};

// Thread group size: 32 threads per group
// Each thread processes one element within a Q5_1 block
[numthreads(32, 1, 1)]
void main(uint3 groupId : SV_GroupID, uint threadId : SV_GroupIndex) {
    // Calculate which Q5_1 block this thread group is processing
    uint blockIndex = groupId.x;
    if (blockIndex >= blockCount) return;
    
    // Load the Q5_1 block from input buffer
    block_q5_1 block = load_q5_1_block(quantizedData, blockIndex + inputOffset);
    
    // Each thread in the group processes one element of the block
    uint elementIndex = threadId;  // 0-31
    
    // Calculate global output index
    uint globalIndex = blockIndex * QK5_1 + elementIndex + outputOffset;
    if (globalIndex >= totalElements) return;
    
    // Convert ggml_half to float
    const float scale = ggml_half_to_float(block.d);
    const float min_val = ggml_half_to_float(block.m);

    // GGML Q5_1 dequantization algorithm (matching CPU implementation)
    // Each qs byte contains two 4-bit values, qh contains high bits

    uint j = elementIndex / 2;  // Which byte in qs array (0-15)
    uint is_upper = elementIndex % 2;  // 0 for lower nibble, 1 for upper nibble

    uint qs_byte = extractBits(block.qs[j / 4], (j % 4) * 8, 8);

    uint base_val;
    uint high_bit_offset;

    if (is_upper == 0) {
        // Lower nibble: j + 0
        base_val = qs_byte & 0x0F;
        high_bit_offset = j + 0;
    } else {
        // Upper nibble: j + 12 (offset by qk/2)
        base_val = qs_byte >> 4;
        high_bit_offset = j + 12;
    }

    // Extract high bit from qh
    uint high_bit = ((block.qh >> high_bit_offset) << 4) & 0x10;

    // Combine to get 5-bit value
    uint quantized = base_val | high_bit;

    // Dequantize: value = quantized * scale + min
    float dequantized_value = (float)quantized * scale + min_val;
    
    // Store result in output buffer
    dequantizedData[globalIndex] = dequantized_value;
}
