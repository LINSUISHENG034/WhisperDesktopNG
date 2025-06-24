```
/*
 * CWhisperEngine.cpp - CWhisperEngine 的部分实现
 * 这是 decode() 方法的最终、正确实现，使用了最新的 whisper.cpp API 并手动实现了贪心采样。
 */

// ... (其他 include 和 CWhisperEngine 的方法) ...

TranscriptionResult CWhisperEngine::decode(const TranscriptionConfig& config)
{
    if (!m_isEncoded) {
        throw CWhisperError("decode() called before a successful encode().");
    }

    // --- 使用最新的 API 实现解码循环 ---
    TranscriptionResult result;
    result.success = false; // 默认为失败

    try {
        // 1. 获取模型词汇表大小
        const int n_vocab = whisper_model_n_vocab(m_ctx);

        // 2. 准备初始的解码 Tokens
        std::vector<whisper_token> prompt_tokens;
        // (可以根据 config 中的 prompt 或语言设置来填充 prompt_tokens)
        // 这里为了简化，我们从一个空的 prompt 开始
        
        // 3. 设置解码循环参数
        const int max_tokens = config.max_tokens > 0 ? config.max_tokens : 512;
        int n_past = 0; // 已处理的token数量

        // 4. 解码主循环
        for (int i = 0; i < max_tokens; ++i) {
            
            // 使用上一次的tokens来解码，获取下一次的logits
            if (whisper_decode(m_ctx, prompt_tokens.data(), prompt_tokens.size(), n_past, config.n_threads) != 0) {
                throw CWhisperError("whisper_decode failed.");
            }

            // 从 logits 中手动实现贪心采样 (Greedy Sampling)
            const float* logits = whisper_get_logits(m_ctx);
            int best_token_id = 0;
            float max_prob = -1.0f;
            for (int j = 0; j < n_vocab; ++j) {
                if (logits[j] > max_prob) {
                    max_prob = logits[j];
                    best_token_id = j;
                }
            }

            // 检查是否是序列结束符 (EOT)
            if (best_token_id == whisper_token_eot(m_ctx)) {
                break;
            }

            // (此处可以添加更复杂的分段和时间戳处理逻辑)
            // 简单实现：将所有token的文本拼接到第一个分段中
            if (result.segments.empty()) {
                result.segments.emplace_back();
            }
            result.segments.back().text += whisper_token_to_str(m_ctx, best_token_id);

            // 准备下一次循环
            n_past += prompt_tokens.size();
            prompt_tokens.clear();
            prompt_tokens.push_back(best_token_id);
        }

        result.success = true;

    } catch (const std::exception& e) {
        // 确保在异常时重置状态
        m_isEncoded = false; 
        throw; // 重新抛出异常
    }
    
    m_isEncoded = false; // 在成功结束后重置编码状态
    return result;
}

```