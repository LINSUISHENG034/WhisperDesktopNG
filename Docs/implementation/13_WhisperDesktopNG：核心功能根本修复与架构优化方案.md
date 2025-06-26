# **WhisperDesktopNG：核心功能根本修复与架构优化方案**

**版本**: 1.0

**日期**: 2025-06-26

**目标**: 彻底修复 main.exe 标准转录功能失效的严重缺陷，并为后续架构优化奠定基础。

## **1\. 战略评估**

本方案基于团队提出的“PCM直通路径”策略。该策略被评估为**最佳修复路径**，其核心优势在于：

* **精确打击**: 准确识别出 ContextImpl::runFull 方法是数据流的分叉点，iAudioBuffer 中包含了可直接使用的原始PCM数据。  
* **最小侵入**: 无需对上层调用和核心接口签名进行颠覆性修改，仅在ContextImpl中增加一个逻辑分支，极大地降低了回归风险。  
* **兼容并蓄**: 完美保留了旧的MEL处理路径，确保了DirectComputeEncoder等旧组件的兼容性，做到了新旧分离。

**最终战略决策：采纳“PCM旁路” (PCM Bypass) 策略。即在核心转录上下文中，为新的 WhisperCppEncoder 开辟一条直接处理PCM数据的快速通道，绕过对新引擎无用且有害的MEL转换步骤。**

## **2\. 详细行动计划**

本计划分为五个阶段，旨在以结构化、可验证的方式完成修复。

### **阶段一：接口扩展 (Interface Extension)**

**目标**: 在不破坏现有签名的情况下，为 iWhisperEncoder 接口赋予识别和处理PCM数据的能力。

1. **修改 Whisper/iWhisperEncoder.h**:  
   * 添加一个纯虚函数 supportsPcmInput()，用于查询编码器是否支持PCM直通。  
   * 添加一个纯虚函数 transcribePcm()，作为处理PCM数据的新入口。

// In: Whisper/iWhisperEncoder.h

\#include "iAudioBuffer.h" // 确保包含了iAudioBuffer的定义  
\#include "iTranscribeResult.h"

struct iWhisperEncoder: public ComLight::iUnknown  
{  
    // ... 现有的 encode, encodeOnly, decodeOnly 方法保持不变 ...

    // \[新增\] 查询编码器是否支持PCM直通路径  
    virtual bool supportsPcmInput() const \= 0;

    // \[新增\] 为支持PCM的编码器提供的新转录方法  
    virtual HRESULT transcribePcm( const iAudioBuffer\* buffer,  
                                   const sProgressSink& progress,  
                                   iTranscribeResult\*\* resultSink ) \= 0;  
};

### **阶段二：编码器实现 (Encoder Implementation)**

**目标**: 让新旧两个编码器分别实现新接口，明确各自的能力。

1. **修改 Whisper/WhisperCppEncoder.h & .cpp (新引擎)**:  
   * 在类声明中，重写 supportsPcmInput 和 transcribePcm。  
   * supportsPcmInput() 直接返回 true。  
   * transcribePcm() 的实现逻辑，就是你们计划中的逻辑：从 iAudioBuffer 提取PCM数据，调用 m\_engine-\>transcribe()。

// In: Whisper/WhisperCppEncoder.h  
class WhisperCppEncoder : public iWhisperEncoder {  
public:  
    // ...  
    bool supportsPcmInput() const override { return true; }  
    HRESULT transcribePcm( const iAudioBuffer\* buffer,  
                           const sProgressSink& progress,  
                           iTranscribeResult\*\* resultSink ) override;  
};

// In: Whisper/WhisperCppEncoder.cpp  
HRESULT WhisperCppEncoder::transcribePcm( const iAudioBuffer\* buffer,  
                                         const sProgressSink& progress,  
                                         iTranscribeResult\*\* resultSink )  
{  
    // 该实现与你们计划中的代码完全一致，是正确的  
    try {  
        const float\* pcmData \= buffer-\>getPcmMono();  
        const uint32\_t sampleCount \= buffer-\>countSamples();  
        if( \!pcmData || sampleCount \== 0 )  
            return E\_INVALIDARG;

        // 将裸指针数据安全地复制到std::vector中  
        std::vector\<float\> audioData( pcmData, pcmData \+ sampleCount );

        // 调用已验证成功的PCM转录引擎  
        TranscriptionResult engineResult \= m\_engine-\>transcribe( audioData, m\_config, progress );

        // 转换并返回COM结果对象  
        ComLight::CComPtr\<ComLight::Object\<TranscribeResult\>\> resultObj;  
        CHECK( ComLight::Object\<TranscribeResult\>::create( resultObj ) );  
        CHECK( convertResults( engineResult, \*resultObj ) );  
        return resultObj.detach( resultSink );  
    }  
    catch( const std::exception& e ) {  
        logException( e );  
        return E\_FAIL;  
    }  
}

2. **修改 Whisper/DirectComputeEncoder.h & .cpp (旧引擎)**:  
   * 同样重写两个新方法，但明确表示“不支持”。这是一种良好的防御性编程实践。

// In: Whisper/DirectComputeEncoder.h  
class DirectComputeEncoder : public iWhisperEncoder {  
public:  
    // ...  
    bool supportsPcmInput() const override { return false; }  
    HRESULT transcribePcm( const iAudioBuffer\* buffer,  
                           const sProgressSink& progress,  
                           iTranscribeResult\*\* resultSink ) override;  
};

// In: Whisper/DirectComputeEncoder.cpp  
HRESULT DirectComputeEncoder::transcribePcm( ... )  
{  
    // 明确表示不支持此功能  
    return E\_NOTIMPL;  
}

### **阶段三：核心逻辑修改 (Core Logic Modification)**

**目标**: 在 ContextImpl 中实现“PCM旁路”逻辑。

1. **修改 Whisper/ContextImpl.misc.cpp**:  
   * 在 runFull 方法的开始处，加入你们设计的 if 判断分支。

// In: Whisper/ContextImpl.misc.cpp  
HRESULT COMLIGHTCALL ContextImpl::runFull( const sFullParams& params, const iAudioBuffer\* buffer )  
{  
    // \[核心修改\] PCM旁路逻辑  
    if( m\_encoder && m\_encoder-\>supportsPcmInput() )  
    {  
        // 使用新的PCM直通路径  
        logMessage( "ContextImpl::runFull: Using PCM direct path for WhisperCppEncoder." );  
        m\_result.clear();

        ComLight::CComPtr\<iTranscribeResult\> transcribeResult;  
        // 进度回调暂时传空，可根据需要实现  
        sProgressSink progressSink{ nullptr, nullptr };  
        CHECK( m\_encoder-\>transcribePcm( buffer, progressSink, \&transcribeResult ) );

        // 转换结果并存储  
        CHECK( this-\>convertResult( transcribeResult, m\_result ) );  
        logMessage( "ContextImpl::runFull: PCM transcription completed successfully." );  
        return S\_OK;  
    }

    // \[保持不变\] 为旧引擎保留的MEL转换路径  
    logMessage( "ContextImpl::runFull: Using legacy MEL path." );  
    // ... 原有的 pcmToMel() 和后续调用逻辑 ...  
}

### **阶段四：验证与测试 (Verification & Testing)**

**目标**: 确保修复有效，且未引入任何功能回退。

1. **编译**: 编译整个解决方案，确保无编译错误。  
2. **功能验证 (核心)**:  
   * 执行标准转录命令: .\\main.exe \-m "model.bin" \-f "jfk.wav" \-l en \-otxt  
   * **验收标准**:  
     * jfk.txt 文件被正确生成，内容不再为空。  
     * 转录文本与 \--test-pcm 路径的输出完全一致。  
3. **全面回归测试**:  
   * 测试所有输出格式：-osrt 和 \-ovtt。  
   * 测试长音频文件 (columbia\_converted.wav)。  
   * (可选) 如果可以切换回 DirectComputeEncoder，验证其功能依然正常。  
   * **性能基准**: 记录转录时间，应与 \--test-pcm 路径基本持平。

### **阶段五：清理与巩固 (Cleanup & Consolidation)**

**目标**: 移除冗余代码，巩固胜利果实。

1. **移除冗余测试代码**:  
   * 一旦 main.exe 标准路径被验证成功，--test-pcm 参数及其关联的 testPcmTranscription 函数就完成了历史使命，可以安全地从 main.cpp 和 whisperCom.cpp 中移除。  
2. **清理技术债务**:  
   * CWhisperEngine 中用于处理MEL数据的 transcribeFromMel() 方法现在已不再被调用，可以移除。  
   * WhisperCppEncoder 中的 extractMelData() 同样可以移除。  
3. **添加代码注释**: 在 ContextImpl::runFull 的 if 分支处添加注释，解释为什么这里需要一个旁路，以方便未来维护。

## **3\. 风险与收益**

* **风险**: **极低**。本方案保留了原有架构的主干，仅通过一个逻辑分支进行扩展，对现有功能的影响被控制在最小范围。  
* **收益**: **巨大**。  
  * **功能恢复**: 项目核心的命令行转录功能将100%恢复正常。  
  * **架构优化**: 为新引擎建立了正确、高效的数据通道，实现了新旧技术栈的清晰分离。  
  * **信心提振**: 解决了一个长期存在的关键缺陷，为项目后续发展扫清了最大障碍。

## **4\. 结论**

本方案是一个务实、高效且低风险的修复计划。它凝聚了团队的智慧，并结合了架构设计的最佳实践。严格按照此计划执行，**必将彻底解决 main.exe 转录失败的问题**，让项目重回正轨。