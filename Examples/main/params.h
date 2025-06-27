#pragma once
#include <vector>
#include <string>

// command-line parameters
struct whisper_params
{
	uint32_t n_threads;
	uint32_t n_processors = 1;
	uint32_t offset_t_ms = 0;
	uint32_t offset_n = 0;
	uint32_t duration_ms = 0;
	uint32_t max_context = UINT_MAX;
	uint32_t max_len = 0;

	float word_thold = 0.01f;

	// 新增：whisper.cpp关键参数
	float entropy_thold = 2.40f;      // 熵阈值
	float logprob_thold = -1.00f;     // 对数概率阈值
	float no_speech_thold = 0.60f;    // 无语音阈值
	float temperature = 0.00f;        // 采样温度
	int32_t best_of = 5;              // greedy采样best_of
	int32_t beam_size = 5;            // beam search宽度

	bool speed_up = false;
	bool translate = false;
	bool diarize = false;
	bool output_txt = false;
	bool output_vtt = false;
	bool output_srt = false;
	bool output_wts = false;
	bool print_special = false;
	bool print_colors = true;
	bool no_timestamps = false;
	bool minimal_test = false;  // J.2 TASK: 运行最小化测试
	bool golden_playback_test = false;  // 1.2 TASK: 黄金数据回放测试

	// 新增：whisper.cpp关键布尔参数
	bool detect_language = false;     // 自动语言检测
	bool suppress_blank = true;       // 抑制空白输出
	bool suppress_nst = true;         // 抑制非语音token
	bool no_context = false;          // 不使用历史上下文
	bool single_segment = false;      // 强制单段输出
	bool token_timestamps = false;    // token级时间戳
	bool debug_mode = false;          // 调试模式
	bool split_on_word = false;       // 按词分割

	std::string language = "en";
	std::wstring model = L"models/ggml-base.en.bin";
	std::wstring gpu;
	std::string prompt;
	std::vector<std::wstring> fname_inp;

	whisper_params();

	bool parse( int argc, wchar_t* argv[] );
};

void whisper_print_usage( int argc, wchar_t** argv, const whisper_params& params );