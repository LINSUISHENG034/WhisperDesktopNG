# **WhisperDesktopNG：核心功能根本修复与架构优化方案**

## **版本 1.1: 修正与最终实施 (Correction & Final Implementation)**

**日期**: 2025-06-26

**状态**: 根据验证阶段发现的关键问题进行修正。

### **A. 新问题诊断 (Diagnosis of New Issues)**

在实施v1.0计划后，团队发现“PCM旁路”逻辑并未被触发。根本原因在于 main.exe 的标准调用链并非直接调用 ContextImpl::runFull，而是通过一个更高层的封装（极有可能是 ContextImpl::runStreamed），该封装内部创建了 MelStreamer 并直接调用了更底层的 runFullImpl，从而绕过了我们的修复逻辑。

### **B. 修正后战略：上游拦截 (Revised Strategy: Upstream Interception)**

战略核心保持不变，但战术进行升级。我们必须在调用链的更上游，即在 MelStreamer 被创建**之前**，就对数据流进行拦截和分流。

**修正后决策：在 ContextImpl 的顶层转录入口（如 runStreamed）实施“PCM旁路”。当检测到编码器为 WhisperCppEncoder 时，我们将完全绕过 MelStreamer 的创建，直接从 iAudioReader 中提取完整的PCM数据，并送入新的 transcribePcm 路径。**

### **C. 修正后行动计划**

**前序阶段（已完成）**: 阶段一和阶段二（接口扩展、编码器实现）的成果完全保留，它们是本次修正的基础。

#### **阶段 2.5: 调用链确认 (Call Stack Confirmation) \- \[关键步骤\]**

**目标**: 100%确认 main.exe 中标准文件转录的实际调用入口。

1. **使用调试器**: 在 main.cpp 中发起转录的位置设置断点。  
2. **单步跟踪**: 步入（Step Into）context-\>... 的调用，跟踪并记录完整的函数调用栈，直到进入 ContextImpl 的某个方法。**请务必确认这个入口方法，后续修改将在此进行。** (我们高度怀疑它是 runStreamed)。

#### **阶段三 (修正版): 上游逻辑修改 (Upstream Logic Modification)**

**目标**: 在上一步确认的真实入口方法中，实现“PCM旁路”。以下以 runStreamed 为例。

1. **修改 Whisper/ContextImpl.cpp 中 runStreamed 方法**:  
   // In: Whisper/ContextImpl.cpp  
   // 假设入口方法为 runStreamed  
   HRESULT COMLIGHTCALL ContextImpl::runStreamed( const sFullParams& params, iAudioReader\* reader, const sProgressSink& progress )  
   {  
       // \[核心修正\] 在调用链上游进行拦截  
       if( m\_encoder && m\_encoder-\>supportsPcmInput() )  
       {  
           logMessage( "ContextImpl::runStreamed: WhisperCppEncoder detected. Engaging PCM direct path." );  
           try  
           {  
               // 1\. 从 iAudioReader 中提取完整的PCM数据  
               // 我们需要一个辅助类来读取所有流式数据  
               PcmReader pcmReader( \*reader );  
               std::vector\<float\> pcmAudio;  
               // 预分配内存以提高效率  
               pcmAudio.reserve( (size\_t)( reader-\>getDuration() \* 16000.0 \* 1.1 ) );

               std::vector\<float\> chunk;  
               while( pcmReader.readChunk( chunk ) )  
               {  
                   pcmAudio.insert( pcmAudio.end(), chunk.begin(), chunk.end() );  
               }

               if( pcmAudio.empty() )  
               {  
                   logMessage( "ContextImpl::runStreamed: ERROR \- Failed to read any PCM data from iAudioReader." );  
                   return E\_FAIL;  
               }

               // 2\. 将完整的PCM数据封装成 iAudioBuffer  
               // 创建一个临时的、实现了 iAudioBuffer 的对象  
               CComPtr\<iAudioBuffer\> pcmBuffer \= createAudioBuffer( pcmAudio );

               // 3\. 调用我们已实现的PCM转录方法  
               m\_result.clear();  
               ComLight::CComPtr\<iTranscribeResult\> transcribeResult;  
               CHECK( m\_encoder-\>transcribePcm( pcmBuffer, progress, \&transcribeResult ) );  
               CHECK( this-\>convertResult( transcribeResult, m\_result ) );

               logMessage( "ContextImpl::runStreamed: PCM transcription completed successfully." );  
               return S\_OK;  
           }  
           catch( const std::exception& e )  
           {  
               logException( e );  
               return E\_FAIL;  
           }  
       }

       // \[保持不变\] 为旧引擎保留的MEL转换路径  
       logMessage( "ContextImpl::runStreamed: Using legacy MEL path for DirectComputeEncoder." );  
       // ... 原有的创建 MelStreamer 并调用 runFullImpl 的逻辑 ...  
       // 例如: return runFullImpl( params, ComLight::Object\<MelStreamer\>::create( ... ) );  
   }

   *注意: createAudioBuffer 是一个需要我们实现的辅助函数，它创建一个实现了iAudioBuffer接口的简单对象，其内部持有std::vector\<float\>的数据。*

#### **阶段四 & 五 (保持不变): 验证与清理**

* **验证**: 现在的核心验证目标是，在执行标准转录命令时，调试日志应明确显示 "Engaging PCM direct path."。后续所有验收标准与v1.0一致。  
* **清理**: 计划不变。一旦新路径验证成功，即可清理技术债务。

## **版本 1.0: 初始修复计划 (Archived)**

*(v1.0的原始内容作为历史记录保留在此处...)*

### **1\. 战略评估**

...  
(以下内容省略)