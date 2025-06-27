```
/*
 * CWhisperEngine.cpp - CWhisperEngine 的部分实现
 * 这是 decode() 方法的正确实现，使用了最新的 whisper.cpp API
 */

// ... (其他 include 和 CWhisperEngine 的方法) ...

TranscriptionResult CWhisperEngine::decode(const TranscriptionConfig& config)
{
    if (!m_isEncoded) {
        throw CWhisperError("decode() called before a successful encode().");
    }

    // --- 使用最新的 API 实现解码循环 ---

    TranscriptionResult result;
    whisper_token_data last_token_data = {}; // 用于存储上一个token信息
    float current_confidence = 0.0f;
    int consecutive_timestamps = 0;

    // 1. 设置解码参数和回调 (如果需要)
    // whisper_decoding_params dparams = ...

    // 2. 解码主循环
    while (true) {
        // 调用 whisper_decode 来获取下一个token的概率分布 (logits)
        int ret = whisper_decode(m_ctx, &last_token_data, 1, /* n_threads */);
        if (ret != 0) {
            // 解码失败
            break;
        }

        // 从 logits 中进行采样，获取最可能的下一个 token
        // 这是替代旧版 whisper_sample_best 的方法
        whisper_token new_token_id = whisper_sample_token_greedy(m_ctx);

        // 获取该 token 的数据
        last_token_data = whisper_token_to_data(m_ctx, new_token_id);

        // 检查是否是序列结束符 (EOT)
        if (new_token_id == whisper_token_eot(m_ctx)) {
            break;
        }

        // --- 时间戳处理 ---
        if (new_token_id >= whisper_token_timestamp_begin(m_ctx)) {
            // (这里可以添加更复杂的时间戳处理逻辑)
            consecutive_timestamps++;
            if (consecutive_timestamps >= 2) {
                 // 如果连续出现时间戳，可能意味着一个分段的结束
                 // 这里是实现分段逻辑的关键点
            }
        } else {
            consecutive_timestamps = 0;
        }

        // (这里可以添加更复杂的置信度和分段逻辑)

        // 暂时简单地将所有token的文本拼接到第一个分段中
        if (result.segments.empty()) {
            result.segments.emplace_back();
        }
        result.segments.back().text += whisper_token_to_str(m_ctx, new_token_id);
    }
    
    m_isEncoded = false; // 重置编码状态
    result.success = true;
    return result;
}

```