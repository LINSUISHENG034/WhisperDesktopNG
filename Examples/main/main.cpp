#include "params.h"
#include "../../Whisper/API/iContext.cl.h"
#include "../../Whisper/API/iMediaFoundation.cl.h"
#include "../../ComLightLib/comLightClient.h"
#include "miscUtils.h"
#include <array>
#include <atomic>
#include "textWriter.h"

// J.2 TASK: 添加whisper.cpp官方音频加载支持
#include "../../external/whisper.cpp/examples/common-whisper.h"
#include "../../external/whisper.cpp/include/whisper.h"

// Include for direct PCM transcription test
#include "../../Whisper/CWhisperEngine.h"

// Declare the exported test function
extern "C" int testPcmTranscription(const char* modelPath, const char* audioPath);

using namespace Whisper;

// J.2 TASK: 函数声明
int RunMinimalTest(const std::string& modelPath, const std::string& audioPath);

#define STREAM_AUDIO 0  // [TEMPORARY] Force buffered mode to test PCM bypass

static HRESULT loadWhisperModel( const wchar_t* path, const std::wstring& gpu, iModel** pp )
{
	using namespace Whisper;
	sModelSetup setup;
	setup.impl = eModelImplementation::GPU;
	if( !gpu.empty() )
		setup.adapter = gpu.c_str();

	// B.1 LOG: main.cpp加载模型
	printf("[DEBUG] main.cpp: loadWhisperModel calling Whisper::loadModel with GPU implementation\n");
	fflush(stdout);

	HRESULT hr = Whisper::loadModel( path, setup, nullptr, pp );

	printf("[DEBUG] main.cpp: Whisper::loadModel returned hr=0x%08X\n", hr);
	fflush(stdout);

	return hr;
}

namespace
{
	// Terminal color map. 10 colors grouped in ranges [0.0, 0.1, ..., 0.9]
	// Lowest is red, middle is yellow, highest is green.
	static const std::array<const char*, 10> k_colors =
	{
		"\033[38;5;196m", "\033[38;5;202m", "\033[38;5;208m", "\033[38;5;214m", "\033[38;5;220m",
		"\033[38;5;226m", "\033[38;5;190m", "\033[38;5;154m", "\033[38;5;118m", "\033[38;5;82m",
	};

	std::string to_timestamp( sTimeSpan ts, bool comma = false )
	{
		sTimeSpanFields fields = ts;
		uint32_t msec = fields.ticks / 10'000;
		uint32_t hr = fields.days * 24 + fields.hours;
		uint32_t min = fields.minutes;
		uint32_t sec = fields.seconds;

		char buf[ 32 ];
		snprintf( buf, sizeof( buf ), "%02d:%02d:%02d%s%03d", hr, min, sec, comma ? "," : ".", msec );
		return std::string( buf );
	}

	static int colorIndex( const sToken& tok )
	{
		const float p = tok.probability;
		const float p3 = p * p * p;
		int col = (int)( p3 * float( k_colors.size() ) );
		col = std::max( 0, std::min( (int)k_colors.size() - 1, col ) );
		return col;
	}

	HRESULT __cdecl newSegmentCallback( iContext* context, uint32_t n_new, void* user_data ) noexcept
	{
		// B.1 LOG: newSegmentCallback被调用
		printf("[DEBUG] newSegmentCallback: Called with n_new=%u\n", n_new);
		fflush(stdout);

		ComLight::CComPtr<iTranscribeResult> results;
		CHECK( context->getResults( eResultFlags::Timestamps | eResultFlags::Tokens, &results ) );

		sTranscribeLength length;
		CHECK( results->getSize( length ) );

		const whisper_params& params = *( (const whisper_params*)user_data );

		// print the last n_new segments
		const uint32_t s0 = length.countSegments - n_new;
		if( s0 == 0 )
			printf( "\n" );

		const sSegment* const segments = results->getSegments();
		const sToken* const tokens = results->getTokens();

		for( uint32_t i = s0; i < length.countSegments; i++ )
		{
			const sSegment& seg = segments[ i ];

			if( params.no_timestamps )
			{
				if( params.print_colors )
				{
					for( uint32_t j = 0; j < seg.countTokens; j++ )
					{
						const sToken& tok = tokens[ seg.firstToken + j ];
						if( !params.print_special && ( tok.flags & eTokenFlags::Special ) )
							continue;
						wprintf( L"%S%s%S", k_colors[ colorIndex( tok ) ], utf16( tok.text ).c_str(), "\033[0m" );
					}
				}
				else
					wprintf( L"%s", utf16( seg.text ).c_str() );
				fflush( stdout );
				continue;
			}

			std::string speaker = "";

			if( params.diarize )
			{
				eSpeakerChannel channel;
				HRESULT hr = context->detectSpeaker( seg.time, channel );
				if( SUCCEEDED( hr ) && channel != eSpeakerChannel::NoStereoData )
				{
					using namespace std::string_literals;
					switch( channel )
					{
					case eSpeakerChannel::Unsure:
						speaker = "(speaker ?)"s;
						break;
					case eSpeakerChannel::Left:
						speaker = "(speaker 0)"s;
						break;
					case eSpeakerChannel::Right:
						speaker = "(speaker 1)";
						break;
					}
				}
			}

			if( params.print_colors )
			{
				printf( "[%s --> %s] %s ",
					to_timestamp( seg.time.begin ).c_str(),
					to_timestamp( seg.time.end ).c_str(),
					speaker.c_str() );

				for( uint32_t j = 0; j < seg.countTokens; j++ )
				{
					const sToken& tok = tokens[ seg.firstToken + j ];
					if( !params.print_special && ( tok.flags & eTokenFlags::Special ) )
						continue;
					wprintf( L"%S%s%S", k_colors[ colorIndex( tok ) ], utf16( tok.text ).c_str(), "\033[0m" );
				}
				printf( "\n" );
			}
			else
				wprintf( L"[%S --> %S]  %S%s\n", to_timestamp( seg.time.begin ).c_str(), to_timestamp( seg.time.end ).c_str(), speaker.c_str(), utf16( seg.text ).c_str() );
		}
		return S_OK;
	}

	HRESULT __cdecl beginSegmentCallback( iContext* context, void* user_data ) noexcept
	{
		std::atomic_bool* flag = (std::atomic_bool*)user_data;
		bool aborted = flag->load();
		return aborted ? S_FALSE : S_OK;
	}

	HRESULT setupConsoleColors()
	{
		HANDLE h = GetStdHandle( STD_OUTPUT_HANDLE );
		if( h == INVALID_HANDLE_VALUE )
			return HRESULT_FROM_WIN32( GetLastError() );

		DWORD mode = 0;
		if( !GetConsoleMode( h, &mode ) )
			return HRESULT_FROM_WIN32( GetLastError() );
		if( 0 != ( mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING ) )
			return S_FALSE;

		mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		if( !SetConsoleMode( h, mode ) )
			return HRESULT_FROM_WIN32( GetLastError() );
		return S_OK;
	}
}

static void __stdcall setPrompt( const int* ptr, int length, void* pv )
{
	std::vector<int>& vec = *( std::vector<int> * )( pv );
	if( length > 0 )
		vec.assign( ptr, ptr + length );
}

int wmain( int argc, wchar_t* argv[] )
{
	// EARLY DEBUG: Program started
	printf("[DEBUG] main.cpp: Program started - wmain() entry point\n");
	fflush(stdout);

	// SPECIAL TEST: Direct PCM transcription test
	if (argc == 4 && wcscmp(argv[1], L"--test-pcm") == 0) {
		printf("[DEBUG] main.cpp: SPECIAL TEST MODE - Direct PCM transcription\n");
		fflush(stdout);

		try {
			// Convert wide strings to narrow strings
			std::wstring wModelPath(argv[2]);
			std::wstring wAudioPath(argv[3]);
			std::string modelPath(wModelPath.begin(), wModelPath.end());
			std::string audioPath(wAudioPath.begin(), wAudioPath.end());

			printf("[DEBUG] main.cpp: Model: %s\n", modelPath.c_str());
			printf("[DEBUG] main.cpp: Audio: %s\n", audioPath.c_str());
			fflush(stdout);

			// Call the exported test function
			int result = testPcmTranscription(modelPath.c_str(), audioPath.c_str());

			return result;
		}
		catch (const std::exception& e) {
			printf("[ERROR] PCM test failed: %s\n", e.what());
			return 1;
		}
	}

	// Whisper::dbgCompareTraces( LR"(C:\Temp\2remove\Whisper\ref.bin)", LR"(C:\Temp\2remove\Whisper\gpu.bin )" ); return 0;

	// Tell logger to use the standard output stream for the messages
	{
		Whisper::sLoggerSetup logSetup;
		logSetup.flags = eLoggerFlags::UseStandardError;
		logSetup.level = eLogLevel::Debug;
		Whisper::setupLogger( logSetup );
	}

	whisper_params params;
	if( !params.parse( argc, argv ) )
		return 1;

	// J.2 TASK: 如果设置了--minimal-test标志，则运行独立测试
	if (params.minimal_test) {
		printf("[DEBUG] J.2 TASK: --minimal-test flag detected, running minimal test\n");

		// 转换路径为std::string
		std::string modelPath;
		std::string audioPath;

		// 转换模型路径
		int modelPathLen = WideCharToMultiByte(CP_UTF8, 0, params.model.c_str(), -1, nullptr, 0, nullptr, nullptr);
		modelPath.resize(modelPathLen - 1);
		WideCharToMultiByte(CP_UTF8, 0, params.model.c_str(), -1, &modelPath[0], modelPathLen, nullptr, nullptr);

		// 转换音频文件路径
		if (!params.fname_inp.empty()) {
			int audioPathLen = WideCharToMultiByte(CP_UTF8, 0, params.fname_inp[0].c_str(), -1, nullptr, 0, nullptr, nullptr);
			audioPath.resize(audioPathLen - 1);
			WideCharToMultiByte(CP_UTF8, 0, params.fname_inp[0].c_str(), -1, &audioPath[0], audioPathLen, nullptr, nullptr);
		} else {
			printf("[ERROR] J.2 TASK: No audio file specified\n");
			return 1;
		}

		return RunMinimalTest(modelPath, audioPath);
	}

	if( params.print_colors )
	{
		if( FAILED( setupConsoleColors() ) )
			params.print_colors = false;
	}

	if( params.fname_inp.empty() )
	{
		fprintf( stderr, "error: no input files specified\n" );
		whisper_print_usage( argc, argv, params );
		return 2;
	}

	if( Whisper::findLanguageKeyA( params.language.c_str() ) == UINT_MAX )
	{
		fprintf( stderr, "error: unknown language '%s'\n", params.language.c_str() );
		whisper_print_usage( argc, argv, params );
		return 3;
	}

	ComLight::CComPtr<iModel> model;
	HRESULT hr = loadWhisperModel( params.model.c_str(), params.gpu, &model );
	if( FAILED( hr ) )
	{
		printError( "failed to load the model", hr );
		return 4;
	}

	std::vector<int> prompt;
	if( !params.prompt.empty() )
	{
		hr = model->tokenize( params.prompt.c_str(), &setPrompt, &prompt );
		if( FAILED( hr ) )
		{
			printError( "failed to tokenize the initial prompt", hr );
			return 5;
		}
	}

	ComLight::CComPtr<iContext> context;
	// B.1 LOG: main.cpp调用createContext
	printf("[DEBUG] main.cpp: Calling model->createContext()\n");
	hr = model->createContext( &context );

	// CRITICAL FIX: 强制设置语言为英语，避免自动检测的不确定性
	printf("[DEBUG] main.cpp: FORCING language to English to avoid auto-detection uncertainty\n");
	params.language = "en";  // 强制英语
	if( FAILED( hr ) )
	{
		printError( "failed to initialize whisper context", hr );
		return 6;
	}
	// B.1 LOG: createContext成功
	printf("[DEBUG] main.cpp: model->createContext() succeeded\n");

	ComLight::CComPtr<iMediaFoundation> mf;
	hr = initMediaFoundation( &mf );
	if( FAILED( hr ) )
	{
		printError( "failed to initialize Media Foundation runtime", hr );
		return 7;
	}

	for( const std::wstring& fname : params.fname_inp )
	{
		// print some info about the processing
		{
			if( model->isMultilingual() == S_FALSE )
			{
				if( params.language != "en" || params.translate )
				{
					params.language = "en";
					params.translate = false;
					fprintf( stderr, "%s: WARNING: model is not multilingual, ignoring language and translation options\n", __func__ );
				}
			}
		}

		// run the inference
		Whisper::sFullParams wparams;
		context->fullDefaultParams( eSamplingStrategy::Greedy, &wparams );

		wparams.resetFlag( eFullParamsFlags::PrintRealtime | eFullParamsFlags::PrintProgress );
		wparams.setFlag( eFullParamsFlags::PrintTimestamps, !params.no_timestamps );
		wparams.setFlag( eFullParamsFlags::PrintSpecial, params.print_special );
		wparams.setFlag( eFullParamsFlags::Translate, params.translate );
		// H.1 FIX: Don't force NoContext flag - this was causing empty transcription results
		// Original comment: "When there're multiple input files, assuming they're independent clips"
		// Problem: This unconditionally disabled context, preventing proper transcription
		// wparams.setFlag( eFullParamsFlags::NoContext );
		wparams.language = Whisper::makeLanguageKey( params.language.c_str() );
		wparams.cpuThreads = params.n_threads;
		if( params.max_context != UINT_MAX )
			wparams.n_max_text_ctx = params.max_context;
		wparams.offset_ms = params.offset_t_ms;
		wparams.duration_ms = params.duration_ms;

		wparams.setFlag( eFullParamsFlags::TokenTimestamps, params.output_wts || params.max_len > 0 );
		wparams.thold_pt = params.word_thold;
		wparams.max_len = params.output_wts && params.max_len == 0 ? 60 : params.max_len;

		wparams.setFlag( eFullParamsFlags::SpeedupAudio, params.speed_up );

		if( !prompt.empty() )
		{
			wparams.prompt_tokens = prompt.data();
			wparams.prompt_n_tokens = (int)prompt.size();
		}

		// This callback is called on each new segment
		if( !wparams.flag( eFullParamsFlags::PrintRealtime ) )
		{
			wparams.new_segment_callback = &newSegmentCallback;
			wparams.new_segment_callback_user_data = &params;
		}

		// example for abort mechanism
		// in this example, we do not abort the processing, but we could if the flag is set to true
		// the callback is called before every encoder run - if it returns false, the processing is aborted
		std::atomic_bool is_aborted = false;
		{
			wparams.encoder_begin_callback = &beginSegmentCallback;
			wparams.encoder_begin_callback_user_data = &is_aborted;
		}

		if( STREAM_AUDIO && !wparams.flag( eFullParamsFlags::TokenTimestamps ) )
		{
			ComLight::CComPtr<iAudioReader> reader;
			CHECK( mf->openAudioFile( fname.c_str(), params.diarize, &reader ) );
			sProgressSink progressSink{ nullptr, nullptr };
			hr = context->runStreamed( wparams, progressSink, reader );
		}
		else
		{
			// Token-level timestamps feature is not currently implemented when streaming the audio
			// When these timestamps are requested, fall back to buffered mode.
			ComLight::CComPtr<iAudioBuffer> buffer;
			CHECK( mf->loadAudioFile( fname.c_str(), params.diarize, &buffer ) );
			// [FINAL ATTEMPT] PCM旁路逻辑 - 在main.cpp中实施
			printf("*** FINAL PCM BYPASS ATTEMPT IN MAIN.CPP ***\n");
			fflush(stdout);

			// 尝试获取encoder并检查PCM支持
			// 注意：这需要扩展iContext接口来获取encoder，这里先记录限制
			printf("*** LIMITATION: Cannot access encoder from iContext interface ***\n");
			printf("*** NEED TO EXTEND iContext TO EXPOSE ENCODER ***\n");
			printf("*** FALLING BACK TO ORIGINAL runFull CALL ***\n");
			fflush(stdout);

			// [CRITICAL FIX] 验证context对象有效性
			printf("[CRITICAL] main.cpp: About to call runFull, context=%p\n", context.operator->());
			fflush(stdout);

			// 尝试调用一个简单的方法来验证对象有效性
			try {
				// 先尝试调用一个安全的方法
				printf("[CRITICAL] main.cpp: Testing context validity...\n");
				fflush(stdout);

				// B.1 LOG: main.cpp调用runFull
				printf("[DEBUG] main.cpp: Calling context->runFull() with buffer=%p\n", buffer.operator->());
				fflush(stdout);
				hr = context->runFull( wparams, buffer );
			}
			catch (...) {
				printf("[ERROR] main.cpp: Exception caught during runFull call!\n");
				fflush(stdout);
				hr = E_FAIL;
			}
			// B.1 LOG: runFull完成
			printf("[DEBUG] main.cpp: context->runFull() completed with hr=0x%08X\n", hr);
			fflush(stdout);
		}

		if( FAILED( hr ) )
		{
			printError( "Unable to process audio", hr );
			return 10;
		}

		if( params.output_txt )
		{
			// H.1 DEBUG: Check transcription results before writing
			ComLight::CComPtr<iTranscribeResult> debugResult;
			hr = context->getResults( eResultFlags::Timestamps, &debugResult );
			if( SUCCEEDED( hr ) )
			{
				sTranscribeLength debugLen;
				hr = debugResult->getSize( debugLen );
				if( SUCCEEDED( hr ) )
				{
					printf("[DEBUG] main.cpp: Transcription results - countSegments=%zu\n", debugLen.countSegments);
					if( debugLen.countSegments > 0 )
					{
						const sSegment* segments = debugResult->getSegments();
						printf("[DEBUG] main.cpp: First segment text: '%s'\n", segments[0].text ? segments[0].text : "(null)");
					}
				}
			}

			bool timestamps = !params.no_timestamps;
			hr = writeText( context, fname.c_str(), timestamps );
			if( FAILED( hr ) )
				printError( "Unable to produce the text file", hr );
		}

		if( params.output_srt )
		{
			hr = writeSubRip( context, fname.c_str() );
			if( FAILED( hr ) )
				printError( "Unable to produce the text file", hr );
		}

		if( params.output_vtt )
		{
			hr = writeWebVTT( context, fname.c_str() );
			if( FAILED( hr ) )
				printError( "Unable to produce the text file", hr );
		}
	}

	context->timingsPrint();
	context = nullptr;
	return 0;
}

// J.2 TASK: 独立的最小化测试函数，使用官方whisper.cpp C API
int RunMinimalTest(const std::string& modelPath, const std::string& audioPath)
{
	printf("[DEBUG] J.2 TASK: RunMinimalTest ENTRY - modelPath=%s, audioPath=%s\n",
		   modelPath.c_str(), audioPath.c_str());

	// 1. 使用官方whisper.cpp的read_audio_data函数加载音频
	std::vector<float> pcmf32;
	std::vector<std::vector<float>> pcmf32s;

	printf("[DEBUG] J.2 TASK: Loading audio using official whisper.cpp read_audio_data\n");
	if (!read_audio_data(audioPath, pcmf32, pcmf32s, false)) {
		printf("[ERROR] J.2 TASK: Failed to load audio file with read_audio_data\n");
		return 1;
	}

	printf("[DEBUG] J.2 TASK: Audio loaded successfully - pcmf32.size()=%u\n", (uint32_t)pcmf32.size());

	// 2. 直接使用whisper.cpp C API初始化模型
	printf("[DEBUG] J.2 TASK: Initializing whisper context with modelPath=%s\n", modelPath.c_str());

	struct whisper_context_params cparams = whisper_context_default_params();
	struct whisper_context* ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);

	if (ctx == nullptr) {
		printf("[ERROR] J.2 TASK: Failed to initialize whisper context\n");
		return 2;
	}

	// 3. 设置官方"黄金标准"参数
	printf("[DEBUG] J.2 TASK: Setting up official whisper.cpp parameters\n");
	struct whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

	// CRITICAL FIX: 修正关键参数以确保转录成功
	wparams.print_realtime   = false;
	wparams.print_progress   = false;
	wparams.print_timestamps = false;
	wparams.print_special    = false;
	wparams.translate        = false;
	wparams.language         = "en";
	wparams.detect_language  = false;
	wparams.n_threads        = 4;

	// 修正关键参数
	wparams.no_context = false;        // 官方默认值是false，不是true！
	wparams.suppress_blank = false;    // 不抑制空白，让模型自由输出
	wparams.no_speech_thold = 0.3f;    // 降低阈值，提高语音检测敏感度（默认0.6太高）

	printf("[DEBUG] J.2 TASK: Using CORRECTED parameters - no_context=%s, suppress_blank=%s, no_speech_thold=%.2f\n",
		   wparams.no_context ? "true" : "false",
		   wparams.suppress_blank ? "true" : "false",
		   wparams.no_speech_thold);

	// 4. 执行转录
	printf("[DEBUG] J.2 TASK: Running whisper_full with pure audio data\n");
	int result = whisper_full(ctx, wparams, pcmf32.data(), (int)pcmf32.size());

	printf("[DEBUG] J.2 TASK: whisper_full returned %d\n", result);

	// 5. 检查结果
	const int n_segments = whisper_full_n_segments(ctx);
	printf("[DEBUG] J.2 TASK: whisper_full_n_segments returned %d\n", n_segments);

	if (result == 0 && n_segments > 0) {
		printf("[SUCCESS] J.2 TASK: TRANSCRIPTION SUCCESSFUL! Found %d segments\n", n_segments);
		for (int i = 0; i < n_segments; ++i) {
			const char* text = whisper_full_get_segment_text(ctx, i);
			printf("Segment %d: %s\n", i, text);
		}
		whisper_free(ctx);
		return 0;
	} else {
		printf("[FAILURE] J.2 TASK: No segments produced even with official audio loading and C API\n");
		whisper_free(ctx);
		return 3;
	}
}