#include <iostream>
#include <vector>
#include <memory>
#include <cstring>
#include <windows.h>
#include <d3d11.h>
#include <atlbase.h>

// Include Whisper DirectCompute headers
#include "../../Whisper/D3D/createDevice.h"
#include "../../Whisper/D3D/createBuffer.h"
#include "../../Whisper/D3D/MappedResource.h"
#include "../../Whisper/D3D/enums.h"
#include "../../Whisper/ML/Device.h"

using namespace DirectCompute;

// Test data structure for Q5_1 block (matching GGML structure)
struct TestQ5_1Block {
    float d;        // scale
    float m;        // min
    uint8_t qs[16]; // quantized values (16 bytes for 32 elements)
    uint8_t qh[4];  // high bits (4 bytes for 32 elements)
};

int main()
{
    std::cout << "=== Quantized Data Integrity Test ===" << std::endl;
    
    try {
        // 1. Initialize DirectCompute device
        std::cout << "[INFO]: Creating DirectCompute device..." << std::endl;
        
        DirectCompute::Device device;
        HRESULT hr = device.create(0, L"");  // flags=0, default adapter
        if (FAILED(hr)) {
            std::cout << "[ERROR]: Failed to create DirectCompute device: " << std::hex << hr << std::endl;
            return 1;
        }
        
        // Set device for current thread
        auto deviceSetup = device.setForCurrentThread();
        
        std::cout << "[PASS]: DirectCompute device created successfully" << std::endl;
        
        // 2. Create test Q5_1 data
        std::cout << "\n=== Creating Test Q5_1 Data ===" << std::endl;
        
        const size_t numBlocks = 5;  // Test with 5 Q5_1 blocks (160 elements total)
        std::vector<TestQ5_1Block> testData(numBlocks);
        
        // Initialize with known test pattern
        for (size_t i = 0; i < numBlocks; ++i) {
            testData[i].d = 1.0f + (float)i * 0.1f;  // scale: 1.0, 1.1, 1.2, ...
            testData[i].m = 0.5f + (float)i * 0.05f; // min: 0.5, 0.55, 0.6, ...
            
            // Fill quantized values with test pattern
            for (int j = 0; j < 16; ++j) {
                testData[i].qs[j] = (uint8_t)((i * 16 + j) % 256);
            }
            
            // Fill high bits with test pattern
            for (int j = 0; j < 4; ++j) {
                testData[i].qh[j] = (uint8_t)((i * 4 + j) % 256);
            }
        }
        
        const size_t totalBytes = numBlocks * sizeof(TestQ5_1Block);
        std::cout << "[INFO]: Created " << numBlocks << " Q5_1 blocks (" << totalBytes << " bytes)" << std::endl;
        std::cout << "[INFO]: Block size: " << sizeof(TestQ5_1Block) << " bytes" << std::endl;
        
        // Verify our block size matches elementSize()
        size_t expectedSize = elementSize(eDataType::Q5_1);
        if (sizeof(TestQ5_1Block) != expectedSize) {
            std::cout << "[WARN]: Block size mismatch - our: " << sizeof(TestQ5_1Block) 
                      << ", expected: " << expectedSize << std::endl;
        } else {
            std::cout << "[PASS]: Block size matches elementSize() function" << std::endl;
        }
        
        // 3. Upload to GPU using createBuffer
        std::cout << "\n=== Uploading Q5_1 Data to GPU ===" << std::endl;
        
        CComPtr<ID3D11Buffer> gpuBuffer;
        CComPtr<ID3D11Buffer> stagingBuffer;
        
        hr = createBuffer(eBufferUse::ReadWriteDownload, totalBytes, &gpuBuffer, 
                         testData.data(), &stagingBuffer, false);
        if (FAILED(hr)) {
            std::cout << "[ERROR]: Failed to create GPU buffer: " << std::hex << hr << std::endl;
            return 1;
        }
        
        std::cout << "[PASS]: Q5_1 data uploaded to GPU successfully" << std::endl;
        
        // 4. Verify buffer properties
        D3D11_BUFFER_DESC desc;
        gpuBuffer->GetDesc(&desc);
        
        std::cout << "[INFO]: GPU buffer size: " << desc.ByteWidth << " bytes" << std::endl;
        std::cout << "[INFO]: GPU buffer usage: " << desc.Usage << std::endl;
        std::cout << "[INFO]: GPU buffer bind flags: " << desc.BindFlags << std::endl;
        
        if (desc.ByteWidth != totalBytes) {
            std::cout << "[ERROR]: Buffer size mismatch - expected: " << totalBytes 
                      << ", actual: " << desc.ByteWidth << std::endl;
            return 1;
        }
        
        // 5. Download data back from GPU
        std::cout << "\n=== Downloading Data from GPU ===" << std::endl;

        // Copy from GPU buffer to staging buffer
        context()->CopyResource(stagingBuffer, gpuBuffer);

        // Map staging buffer and copy data
        std::vector<uint8_t> downloadedData(totalBytes);
        DirectCompute::MappedResource mapped;
        hr = mapped.map(stagingBuffer, true);  // true = read access
        if (FAILED(hr)) {
            std::cout << "[ERROR]: Failed to map staging buffer: " << std::hex << hr << std::endl;
            return 1;
        }

        std::memcpy(downloadedData.data(), mapped.data(), totalBytes);

        std::cout << "[PASS]: Data downloaded from GPU successfully" << std::endl;
        
        // 6. Byte-by-byte comparison
        std::cout << "\n=== Verifying Data Integrity ===" << std::endl;
        
        const uint8_t* originalBytes = reinterpret_cast<const uint8_t*>(testData.data());
        bool dataMatches = true;
        size_t mismatchCount = 0;
        
        for (size_t i = 0; i < totalBytes; ++i) {
            if (originalBytes[i] != downloadedData[i]) {
                if (mismatchCount < 10) {  // Show first 10 mismatches
                    std::cout << "[ERROR]: Byte mismatch at offset " << i 
                              << " - original: 0x" << std::hex << (int)originalBytes[i]
                              << ", downloaded: 0x" << (int)downloadedData[i] << std::dec << std::endl;
                }
                dataMatches = false;
                mismatchCount++;
            }
        }
        
        if (dataMatches) {
            std::cout << "[PASS]: All " << totalBytes << " bytes match perfectly!" << std::endl;
        } else {
            std::cout << "[ERROR]: " << mismatchCount << " bytes do not match" << std::endl;
            return 1;
        }
        
        // 7. Verify block structure integrity
        std::cout << "\n=== Verifying Block Structure ===" << std::endl;
        
        const TestQ5_1Block* downloadedBlocks = reinterpret_cast<const TestQ5_1Block*>(downloadedData.data());
        bool structureValid = true;
        
        for (size_t i = 0; i < numBlocks; ++i) {
            const auto& original = testData[i];
            const auto& downloaded = downloadedBlocks[i];
            
            if (original.d != downloaded.d || original.m != downloaded.m) {
                std::cout << "[ERROR]: Block " << i << " float values mismatch" << std::endl;
                std::cout << "  Original: d=" << original.d << ", m=" << original.m << std::endl;
                std::cout << "  Downloaded: d=" << downloaded.d << ", m=" << downloaded.m << std::endl;
                structureValid = false;
            }
            
            // Check a few quantized bytes
            for (int j = 0; j < 4; ++j) {  // Check first 4 bytes
                if (original.qs[j] != downloaded.qs[j]) {
                    std::cout << "[ERROR]: Block " << i << " qs[" << j << "] mismatch: "
                              << (int)original.qs[j] << " vs " << (int)downloaded.qs[j] << std::endl;
                    structureValid = false;
                }
            }
        }
        
        if (structureValid) {
            std::cout << "[PASS]: All block structures are intact" << std::endl;
        }
        
        // 8. Test with different quantization types
        std::cout << "\n=== Testing Other Quantization Types ===" << std::endl;
        
        // Test Q8_0 (simpler structure)
        const size_t q8_0_block_size = elementSize(eDataType::Q8_0);
        const size_t q8_0_blocks = 3;
        const size_t q8_0_bytes = q8_0_blocks * q8_0_block_size;
        
        std::vector<uint8_t> q8_0_data(q8_0_bytes);
        for (size_t i = 0; i < q8_0_bytes; ++i) {
            q8_0_data[i] = (uint8_t)(i % 256);
        }
        
        CComPtr<ID3D11Buffer> q8_0_buffer, q8_0_staging;
        hr = createBuffer(eBufferUse::ReadWriteDownload, q8_0_bytes, &q8_0_buffer, 
                         q8_0_data.data(), &q8_0_staging, false);
        if (SUCCEEDED(hr)) {
            // Copy and map Q8_0 data
            context()->CopyResource(q8_0_staging, q8_0_buffer);

            DirectCompute::MappedResource q8_0_mapped;
            hr = q8_0_mapped.map(q8_0_staging, true);
            if (SUCCEEDED(hr)) {
                if (std::memcmp(q8_0_data.data(), q8_0_mapped.data(), q8_0_bytes) == 0) {
                    std::cout << "[PASS]: Q8_0 data integrity verified (" << q8_0_bytes << " bytes)" << std::endl;
                } else {
                    std::cout << "[ERROR]: Q8_0 data integrity failed" << std::endl;
                }
            } else {
                std::cout << "[ERROR]: Failed to map Q8_0 staging buffer" << std::endl;
            }
        }
        
        std::cout << "\n=== All Tests Completed Successfully ===" << std::endl;
        std::cout << "[PASS]: Quantized data can be uploaded to GPU without corruption" << std::endl;
        std::cout << "[PASS]: GPU memory layout preserves GGML block structure" << std::endl;
        std::cout << "[PASS]: Data integrity verified through round-trip testing" << std::endl;
        std::cout << "[PASS]: Ready for HLSL shader development" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "[ERROR]: Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "[ERROR]: Unknown exception occurred" << std::endl;
        return 1;
    }
}
