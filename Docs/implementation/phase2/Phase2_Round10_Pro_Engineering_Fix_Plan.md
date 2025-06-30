# **V5.0 终局技术方案：多语言解码器语境预设**

## **1. 执行摘要 (Executive Summary)**

**问题定性**: 项目当前在处理非英语音频时，转录功能完全失效。经过多轮排查，根源已明确：**解码器在初始化时，未能接收到目标语言的语境预设（Context Priming），导致其状态机默认进入英文解码路径。** 这并非一个采样算法或模型能力的缺陷，而是一个在解码流程入口处，初始上下文构建逻辑的确定性错误。

**解决方案**: 本方案将指导团队实现一个符合`whisper.cpp`参考实现的、健壮的初始上下文构建机制。核心是在解码循环开始前，根据目标语言，将正确的语言Token（如`<|zh|>`）前置到提示（prompt）序列中。此举将从根本上修正解码器的初始状态，使其能够正确调用模型的多语言能力。

**预期成果**: 实施本方案后，项目将完全恢复并超越原`Const-me/Whisper`项目对多语言音频（特别是中文）的处理能力，并为未来支持更复杂的解码策略（如翻译任务）奠定坚实的架构基础。

---

## **2. 根本原因分析 (Root Cause Analysis)**

Whisper解码器本质上是一个复杂的状态机，其行为高度依赖于输入的上下文（即历史token序列）。一个标准的多语言转录任务，其初始上下文序列必须严格遵循以下顺序和内容：
`[SOT_TOKEN, LANGUAGE_TOKEN, TASK_TOKEN, TIMESTAMP_TOKENS...]`

我们当前的实现，在构建这个初始序列时，遗漏了至关重要的`LANGUAGE_TOKEN`。其后果是灾难性的：
1.  **状态机初始状态错误**: 在没有明确语言指定的条件下，模型会回退到其最强大的默认状态——英文转录。
2.  **特征空间与假设空间不匹配**: 解码器试图将一个非英语音频的梅尔频谱特征，强行映射到英文的声学-词汇空间上。这必然导致低概率、高熵的`logits`输出。
3.  **“胡言乱语”的产生**: 我们的“胡言乱语”检测器（基于压缩率和对数概率）正确地捕获了这种低质量输出。但这只是症状，而非病因。病因在于解码器从第一步开始，就被“误导”了。

因此，任何在采样阶段（如调整`temperature`或`repetition_penalty`）的努力都注定是徒劳的，因为输入给采样器的`logits`数据本身，就是在一个错误的解码前提下生成的。

---

## **3. 架构决策与方案选型 (Architectural Decision & Rationale)**

**选定方案**: **在解码循环前，实现确定性的、基于目标语言的初始上下文构建。**

**决策依据**:
1.  **直击根源**: 该方案直接修正了状态机的初始状态，解决了问题的根本原因，而非缓解症状。
2.  **架构对齐**: 使我们的实现逻辑与`whisper.cpp`的参考实现保持一致。这种一致性对于未来的维护、升级和性能对比至关重要。
3.  **避免无效优化**: 避免了团队在更下游的采样策略上进行无效的、成本高昂的参数调优，从而节约了宝贵的开发时间。
4.  **最小化技术风险**: 该方案的修改范围被严格限制在`ContextImpl`的初始化逻辑和`iVocab`的接口扩展上，不触及已经稳定运行的GPU计算、张量管理和采样器核心算法，风险可控。

--- 

## **4. 技术实施规约 (Technical Implementation Specification)**

### **任务1: `iVocab` 接口扩展**

*   **规约**: `iVocab`接口必须提供一个通过标准语言代码（ISO 639-1）获取对应语言Token ID的能力。
*   **接口定义 (`Whisper/API/iVocab.h`)**: 
    ```cpp
    // Provides the model-specific token ID for a given language code.
    // @param lang_code A null-terminated string for the language code (e.g., "en", "zh").
    // @return The token ID, or a negative value if the language is not supported.
    virtual int languageTokenId( const char* lang_code ) const = 0;
    ```
*   **实现 (`Whisper/Whisper/VocabImpl.cpp`)**: 实现应直接利用`whisper.cpp`的C-API，并封装其固有的ID计算规则。
    ```cpp
    #include "whisper.h" // Ensure access to the C-API

    int VocabImpl::languageTokenId( const char* lang_code ) const {
        const int lang_id = whisper_lang_id( lang_code );
        if( lang_id < 0 ) 
            return -1;
        // This formula is a fixed convention of the Whisper vocabulary layout.
        return m_vocab.sot_token() + 1 + lang_id;
    }
    ```

### **任务2: `ContextImpl` 上下文预设逻辑实现**

*   **规约**: `runFullImpl`方法必须在进入主解码循环前，根据传入的`sFullParams`，构建一个包含`SOT`, `Language`, `Task`, 和（可选的）`Timestamp` token的完整初始上下文。
*   **实现位置**: `Whisper/Whisper/ContextImpl.cpp`，`runFullImpl`函数体前部。
*   **参考实现**: 
    ```cpp
    // In HRESULT ContextImpl::runFullImpl(...)

    // =================== [ ARCHITECTURAL FIX: CONTEXT PRIMING ] ===================
    m_prompt_tokens.clear();

    // 1. Prepend Start-Of-Transcript token
    m_prompt_tokens.push_back( m_vocab.sot_token() );

    // 2. Prepend Language token
    const char* lang_code = ( params.language == nullptr || strlen( params.language ) == 0 ) ? "en" : params.language;
    const int lang_token_id = m_vocab.languageTokenId( lang_code );
    if( lang_token_id < 0 ) {
        logMessage( eLogLevel::Error, "Unsupported language code: '%s'", lang_code );
        return E_INVALIDARG;
    }
    m_prompt_tokens.push_back( lang_token_id );

    // 3. Prepend Task token
    const int task_token = params.translate ? m_vocab.translate_token() : m_vocab.transcribe_token();
    m_prompt_tokens.push_back( task_token );

    // 4. Prepend Timestamp token if timestamps are enabled
    if( !params.no_timestamps ) {
        m_prompt_tokens.push_back( m_vocab.timestamp_begin_token() );
    }
    // ============================================================================

    // The main decoding loop follows...
    ```

--- 

## **5. 验收标准 (Acceptance Criteria)**

*   **CR1 (Code Implementation)**: `iVocab::languageTokenId`接口已按规约实现。`ContextImpl::runFullImpl`中包含了上述上下文预设逻辑。
*   **CR2 (Functional Correctness - Multilingual)**: 使用`ggml-small.bin`模型和`zh_medium_audio.mp3`测试音频，能够生成高质量、正确的中文转录，且包含准确的时间戳。
*   **CR3 (Regression Test)**: `Tests/comprehensive_transcription_test.ps1`测试集100%通过，确保对英文及其他原有功能的兼容性未受破坏。
*   **CR4 (Deliverable)**: 提供一份由修复后程序生成的、针对`zh_medium_audio.mp3`的、带时间戳的`.txt`转录结果文件，用于与基准进行对比。
