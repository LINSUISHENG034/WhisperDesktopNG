#pragma once
/*
 * UnifiedTranscriptionParams.h - 统一的转录参数系统
 * 
 * 设计目标：
 * 1. 支持Const-me DirectCompute引擎和whisper.cpp引擎的最大公约数参数
 * 2. 为不兼容参数提供统一的维护入口
 * 3. 保持向后兼容性
 * 4. 支持引擎特定的高级参数
 */

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "sFullParams.h"  // Const-me原始参数

namespace Whisper {

// 采样策略 - 两个引擎都支持
enum class UnifiedSamplingStrategy {
    Greedy = 0,
    BeamSearch = 1
};

// 语言设置 - 统一接口
struct LanguageConfig {
    std::string code = "auto";      // 语言代码："auto", "en", "zh", etc.
    bool autoDetect = true;         // 是否启用自动检测
    
    // 转换为Const-me格式
    uint32_t toConstMeFormat() const;
    
    // 转换为whisper.cpp格式
    const char* toWhisperCppFormat() const;
    bool getDetectLanguage() const { return autoDetect && (code == "auto" || code.empty()); }
};

// 核心参数 - 两个引擎都支持
struct CoreParams {
    // 基础设置
    UnifiedSamplingStrategy strategy = UnifiedSamplingStrategy::Greedy;
    int numThreads = 0;                    // 0 = auto-detect
    int maxTextContext = 16384;            // 最大文本上下文
    
    // 时间控制
    int offsetMs = 0;                      // 开始偏移（毫秒）
    int durationMs = 0;                    // 处理时长（毫秒，0=全部）
    
    // 语言设置
    LanguageConfig language;
    
    // 输出控制
    bool translate = false;                // 翻译为英语
    bool enableTimestamps = true;          // 生成时间戳
    bool singleSegment = false;            // 强制单段输出
    bool noContext = false;                // 不使用历史上下文
    
    // 实验性功能
    bool tokenTimestamps = false;          // token级时间戳
    float timestampThreshold = 0.01f;      // 时间戳概率阈值
    float timestampSumThreshold = 0.01f;   // 时间戳概率和阈值
    int maxSegmentLength = 0;              // 最大段长度（字符）
    int maxTokensPerSegment = 0;           // 每段最大token数
    
    // 性能优化
    int audioContext = 0;                  // 音频上下文大小（0=默认）
    
    // 采样参数
    struct {
        int bestOf = 5;                    // greedy采样的best_of
    } greedy;
    
    struct {
        int beamSize = 5;                  // beam search宽度
        float patience = -1.0f;            // beam search耐心值
    } beamSearch;
};

// whisper.cpp特有的高级参数
struct WhisperCppAdvancedParams {
    // VAD (Voice Activity Detection) 参数
    float entropyThreshold = 2.40f;        // 熵阈值
    float logprobThreshold = -1.00f;       // 对数概率阈值
    float noSpeechThreshold = 0.60f;       // 无语音阈值
    
    // 采样控制
    float temperature = 0.0f;              // 采样温度
    float temperatureInc = 0.2f;           // 温度递增
    float maxInitialTimestamp = 1.0f;      // 最大初始时间戳
    float lengthPenalty = -1.0f;           // 长度惩罚
    
    // 抑制控制
    bool suppressBlank = true;             // 抑制空白输出
    bool suppressNonSpeech = true;         // 抑制非语音token
    std::string suppressRegex;             // 抑制正则表达式
    
    // 提示词
    std::string initialPrompt;             // 初始提示词
    std::vector<int> promptTokens;         // 提示token
    
    // 调试和实验
    bool debugMode = false;                // 调试模式
    bool enableTinyDiarize = false;        // 启用说话人分离
    bool splitOnWord = false;              // 按词分割而非token
    
    // 语法约束（高级功能）
    struct GrammarRule {
        std::string name;
        std::string pattern;
    };
    std::vector<GrammarRule> grammarRules;
};

// Const-me特有的参数
struct ConstMeAdvancedParams {
    // 回调函数
    pfnNewSegment newSegmentCallback = nullptr;
    void* newSegmentCallbackUserData = nullptr;
    
    pfnEncoderBegin encoderBeginCallback = nullptr;
    void* encoderBeginCallbackUserData = nullptr;
    
    // 打印控制
    bool printSpecial = false;             // 打印特殊token
    bool printProgress = false;            // 打印进度
    bool printRealtime = false;            // 实时打印
    bool printTimestamps = false;          // 打印时间戳
    
    // 实验性功能
    bool speedupAudio = false;             // 音频加速
};

// 统一参数容器
class UnifiedTranscriptionParams {
public:
    // 核心参数（两个引擎都支持）
    CoreParams core;
    
    // 引擎特定的高级参数
    WhisperCppAdvancedParams whisperCppAdvanced;
    ConstMeAdvancedParams constMeAdvanced;
    
    // 引擎选择提示
    enum class PreferredEngine {
        Auto,           // 自动选择
        ConstMe,        // 优先使用Const-me DirectCompute
        WhisperCpp      // 优先使用whisper.cpp
    } preferredEngine = PreferredEngine::Auto;
    
public:
    // 构造函数
    UnifiedTranscriptionParams() = default;
    
    // 从Const-me参数构造
    explicit UnifiedTranscriptionParams(const sFullParams& constMeParams);
    
    // 从whisper.cpp参数构造
    explicit UnifiedTranscriptionParams(const struct whisper_full_params& whisperParams);
    
    // 转换为Const-me参数
    sFullParams toConstMeParams() const;
    
    // 转换为whisper.cpp参数
    struct whisper_full_params toWhisperCppParams() const;
    
    // 参数验证
    bool validate(std::string& errorMessage) const;
    
    // 获取引擎兼容性信息
    struct EngineCompatibility {
        bool constMeSupported = true;
        bool whisperCppSupported = true;
        std::vector<std::string> constMeUnsupportedFeatures;
        std::vector<std::string> whisperCppUnsupportedFeatures;
    };
    EngineCompatibility getEngineCompatibility() const;
    
    // 参数调试信息
    std::string getDebugInfo() const;
    
    // 预设参数配置
    static UnifiedTranscriptionParams createDefault();
    static UnifiedTranscriptionParams createHighQuality();
    static UnifiedTranscriptionParams createFastTranscription();
    static UnifiedTranscriptionParams createStreamingOptimized();
};

// 参数管理器 - 统一的维护入口
class TranscriptionParamsManager {
public:
    // 加载参数配置
    static UnifiedTranscriptionParams loadFromFile(const std::string& configPath);
    
    // 保存参数配置
    static bool saveToFile(const UnifiedTranscriptionParams& params, const std::string& configPath);
    
    // 从命令行参数解析
    static UnifiedTranscriptionParams parseCommandLine(int argc, char* argv[]);
    
    // 获取参数帮助信息
    static std::string getHelpText();
    
    // 参数验证和建议
    static std::vector<std::string> validateAndSuggest(const UnifiedTranscriptionParams& params);
    
private:
    // 内部配置存储
    static std::unordered_map<std::string, UnifiedTranscriptionParams> s_presetConfigs;
};

} // namespace Whisper
