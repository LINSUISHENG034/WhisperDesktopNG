// GGML Quantization Structures for HLSL
// This file defines HLSL structures that match GGML quantization block layouts
// for GPU-based dequantization operations.

#ifndef GGML_QUANTIZATION_HLSLI
#define GGML_QUANTIZATION_HLSLI

// Constants matching GGML definitions
#define QK4_0 32    // Q4_0 block size
#define QK5_1 32    // Q5_1 block size  
#define QK8_0 32    // Q8_0 block size

// Q4_0 quantization block
// Layout: scale (2 bytes) + quantized values (16 bytes)
// Total: 18 bytes for 32 elements
struct block_q4_0 {
    uint d;             // scale factor (ggml_half as lower 16 bits of uint)
    uint qs[4];         // 16 bytes of quantized values (4 bits per element, packed)
};

// Q5_1 quantization block
// Layout: scale (2 bytes) + min (2 bytes) + high bits (4 bytes) + quantized values (16 bytes)
// Total: 24 bytes for 32 elements
struct block_q5_1 {
    uint d;             // scale factor (ggml_half as lower 16 bits of uint)
    uint m;             // min value (ggml_half as lower 16 bits of uint)
    uint qh;            // 4 bytes of high bits (1 bit per element, packed)
    uint qs[4];         // 16 bytes of quantized values (4 bits per element, packed)
};

// Q8_0 quantization block
// Layout: scale (2 bytes) + quantized values (32 bytes)
// Total: 34 bytes for 32 elements
struct block_q8_0 {
    uint d;             // scale factor (ggml_half as lower 16 bits of uint)
    uint qs[8];         // 32 bytes of quantized values (8 bits per element)
};

// Helper functions for bit manipulation
uint extractBits(uint value, uint offset, uint count) {
    uint mask = (1u << count) - 1u;
    return (value >> offset) & mask;
}

uint setBits(uint value, uint newBits, uint offset, uint count) {
    uint mask = (1u << count) - 1u;
    return (value & ~(mask << offset)) | ((newBits & mask) << offset);
}

// Convert ggml_half (lower 16 bits of uint) to float
float ggml_half_to_float(uint h) {
    // Use HLSL's built-in f16tof32 function
    return f16tof32(h & 0xFFFF);
}

// Q4_0 dequantization function
// Dequantizes a single Q4_0 block to 32 float values
void dequantize_q4_0_block(block_q4_0 block, out float values[QK4_0]) {
    const float scale = block.d;
    
    [unroll]
    for (uint i = 0; i < QK4_0; i++) {
        // Extract 4-bit value from packed data
        uint byteIndex = i / 8;         // Which uint32 contains this element
        uint bitOffset = (i % 8) * 4;   // Bit offset within the uint32
        
        uint quantized = extractBits(block.qs[byteIndex], bitOffset, 4);
        
        // Convert from unsigned to signed 4-bit value
        int signed_val = (int)quantized - 8;
        
        // Dequantize: value = scale * signed_val
        values[i] = scale * (float)signed_val;
    }
}

// Q5_1 dequantization function  
// Dequantizes a single Q5_1 block to 32 float values
void dequantize_q5_1_block(block_q5_1 block, out float values[QK5_1]) {
    const float scale = block.d;
    const float min_val = block.m;
    
    [unroll]
    for (uint i = 0; i < QK5_1; i++) {
        // Extract 4-bit base value from packed data
        uint byteIndex = i / 8;         // Which uint32 contains this element  
        uint bitOffset = (i % 8) * 4;   // Bit offset within the uint32
        
        uint base_val = extractBits(block.qs[byteIndex], bitOffset, 4);
        
        // Extract high bit from qh
        uint high_bit = extractBits(block.qh, i, 1);
        
        // Combine to get 5-bit value
        uint quantized = base_val | (high_bit << 4);
        
        // Dequantize: value = scale * quantized + min
        values[i] = scale * (float)quantized + min_val;
    }
}

// Q8_0 dequantization function
// Dequantizes a single Q8_0 block to 32 float values  
void dequantize_q8_0_block(block_q8_0 block, out float values[QK8_0]) {
    const float scale = block.d;
    
    [unroll]
    for (uint i = 0; i < QK8_0; i++) {
        // Extract 8-bit value from packed data
        uint byteIndex = i / 4;         // Which uint32 contains this element
        uint byteOffset = (i % 4) * 8;  // Byte offset within the uint32
        
        uint quantized = extractBits(block.qs[byteIndex], byteOffset, 8);
        
        // Convert from unsigned to signed 8-bit value
        int signed_val = (int)quantized - 128;
        
        // Dequantize: value = scale * signed_val
        values[i] = scale * (float)signed_val;
    }
}

// Buffer access helpers for ByteAddressBuffer
block_q4_0 load_q4_0_block(ByteAddressBuffer buffer, uint blockIndex) {
    uint offset = blockIndex * 18; // 18 bytes per Q4_0 block

    block_q4_0 block;
    // Load d as 16-bit value (ggml_half)
    block.d = buffer.Load(offset) & 0xFFFF;  // Lower 16 bits

    // Load qs (16 bytes of quantized values)
    block.qs[0] = buffer.Load(offset + 2);
    block.qs[1] = buffer.Load(offset + 6);
    block.qs[2] = buffer.Load(offset + 10);
    block.qs[3] = buffer.Load(offset + 14);

    return block;
}

block_q5_1 load_q5_1_block(ByteAddressBuffer buffer, uint blockIndex) {
    uint offset = blockIndex * 24; // 24 bytes per Q5_1 block

    block_q5_1 block;
    // Load d and m as 16-bit values (ggml_half)
    uint dm_packed = buffer.Load(offset);
    block.d = dm_packed & 0xFFFF;        // Lower 16 bits (scale)
    block.m = (dm_packed >> 16) & 0xFFFF; // Upper 16 bits (min)

    // Load qh (4 bytes of high bits)
    block.qh = buffer.Load(offset + 4);

    // Load qs (16 bytes of quantized values)
    block.qs[0] = buffer.Load(offset + 8);
    block.qs[1] = buffer.Load(offset + 12);
    block.qs[2] = buffer.Load(offset + 16);
    block.qs[3] = buffer.Load(offset + 20);

    return block;
}

block_q8_0 load_q8_0_block(ByteAddressBuffer buffer, uint blockIndex) {
    uint offset = blockIndex * 34; // 34 bytes per Q8_0 block

    block_q8_0 block;
    // Load d as 16-bit value (ggml_half)
    block.d = buffer.Load(offset) & 0xFFFF;  // Lower 16 bits

    // Load qs (32 bytes of quantized values)
    block.qs[0] = buffer.Load(offset + 2);
    block.qs[1] = buffer.Load(offset + 6);
    block.qs[2] = buffer.Load(offset + 10);
    block.qs[3] = buffer.Load(offset + 14);
    block.qs[4] = buffer.Load(offset + 18);
    block.qs[5] = buffer.Load(offset + 22);
    block.qs[6] = buffer.Load(offset + 26);
    block.qs[7] = buffer.Load(offset + 30);

    return block;
}

#endif // GGML_QUANTIZATION_HLSLI
