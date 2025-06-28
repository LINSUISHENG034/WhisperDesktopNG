#pragma once

// Quantization Core Library
// Independent quantization functions extracted from whisper.cpp
// This module provides quantization/dequantization algorithms without the full inference engine

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for GGML types
struct ggml_tensor;

// Quantization function declarations
// These functions are extracted from whisper.cpp to resolve GGML.lib dependencies

// Basic quantization functions
void quantize_q4_0(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_q4_1(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_q5_0(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_q5_1(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_q8_0(const float* src, void* dst, int n, int k, int64_t* hist);

// K-quantization functions
void quantize_q2_K(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_q3_K(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_q4_K(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_q5_K(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_q6_K(const float* src, void* dst, int n, int k, int64_t* hist);

// IQ quantization functions
void quantize_iq1_s(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_iq1_m(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_iq2_xxs(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_iq2_xs(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_iq2_s(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_iq3_xxs(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_iq3_s(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_iq4_nl(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_iq4_xs(const float* src, void* dst, int n, int k, int64_t* hist);

// TQ quantization functions
void quantize_tq1_0(const float* src, void* dst, int n, int k, int64_t* hist);
void quantize_tq2_0(const float* src, void* dst, int n, int k, int64_t* hist);

// Reference quantization functions
void quantize_row_q2_K_ref(const float* x, void* y, int k);
void quantize_row_q3_K_ref(const float* x, void* y, int k);
void quantize_row_q4_0_ref(const float* x, void* y, int k);
void quantize_row_q4_1_ref(const float* x, void* y, int k);
void quantize_row_q4_K_ref(const float* x, void* y, int k);
void quantize_row_q5_0_ref(const float* x, void* y, int k);
void quantize_row_q5_1_ref(const float* x, void* y, int k);
void quantize_row_q5_K_ref(const float* x, void* y, int k);
void quantize_row_q6_K_ref(const float* x, void* y, int k);
void quantize_row_q8_0_ref(const float* x, void* y, int k);
void quantize_row_q8_1_ref(const float* x, void* y, int k);
void quantize_row_iq2_xxs_ref(const float* x, void* y, int k);
void quantize_row_iq2_xs_ref(const float* x, void* y, int k);
void quantize_row_iq2_s_ref(const float* x, void* y, int k);
void quantize_row_iq3_xxs_ref(const float* x, void* y, int k);
void quantize_row_iq3_s_ref(const float* x, void* y, int k);
void quantize_row_iq4_nl_ref(const float* x, void* y, int k);
void quantize_row_iq4_xs_ref(const float* x, void* y, int k);
void quantize_row_tq1_0_ref(const float* x, void* y, int k);
void quantize_row_tq2_0_ref(const float* x, void* y, int k);

// Dequantization functions
void dequantize_row_q4_0(const void* x, float* y, int k);
void dequantize_row_q4_1(const void* x, float* y, int k);
void dequantize_row_q5_0(const void* x, float* y, int k);
void dequantize_row_q5_1(const void* x, float* y, int k);
void dequantize_row_q8_0(const void* x, float* y, int k);
void dequantize_row_q2_K(const void* x, float* y, int k);
void dequantize_row_q3_K(const void* x, float* y, int k);
void dequantize_row_q4_K(const void* x, float* y, int k);
void dequantize_row_q5_K(const void* x, float* y, int k);
void dequantize_row_q6_K(const void* x, float* y, int k);
void dequantize_row_iq1_s(const void* x, float* y, int k);
void dequantize_row_iq1_m(const void* x, float* y, int k);
void dequantize_row_iq2_xxs(const void* x, float* y, int k);
void dequantize_row_iq2_xs(const void* x, float* y, int k);
void dequantize_row_iq2_s(const void* x, float* y, int k);
void dequantize_row_iq3_xxs(const void* x, float* y, int k);
void dequantize_row_iq3_s(const void* x, float* y, int k);
void dequantize_row_iq4_nl(const void* x, float* y, int k);
void dequantize_row_iq4_xs(const void* x, float* y, int k);
void dequantize_row_tq1_0(const void* x, float* y, int k);
void dequantize_row_tq2_0(const void* x, float* y, int k);

// GGML backend functions
void ggml_backend_tensor_set(struct ggml_tensor* tensor, const void* data, size_t offset, size_t size);
void ggml_backend_tensor_memset(struct ggml_tensor* tensor, uint8_t value, size_t offset, size_t size);

// Critical section functions
void ggml_critical_section_start(void);
void ggml_critical_section_end(void);

// IQ initialization functions
void iq2xs_init_impl(void);
void iq2xs_free_impl(void);
void iq3xs_init_impl(void);
void iq3xs_free_impl(void);

#ifdef __cplusplus
}
#endif
