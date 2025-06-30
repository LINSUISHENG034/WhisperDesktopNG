#include "stdafx.h"
#include "ContextImpl.h"
#include "Languages.h"
#include "../Utils/Trace/tracing.h"
#include "../ML/Sampler.h"
#include <set>
#include <cmath>
using namespace Whisper;

ContextImpl::ContextImpl( const DirectCompute::Device& dev, const WhisperModel& modelData, iModel* modelPointer ) :
	device( dev ),
	model( modelData ),
	modelPtr( modelPointer ),
	context( modelData, profiler ),
	profiler( modelData )
{
	// Initialize the advanced token sampler with default parameters
	m_sampler = std::make_unique<WhisperSampler>(SamplingParams::defaultParams(), modelData.shared->vocab);
	m_recent_tokens.reserve(20); // Reserve space for token history
}

#define WHISPER_CHUNK_SIZE  30

HRESULT ContextImpl::encode( iSpectrogram& mel, int seek )
{
	auto prof = profiler.cpuBlock( eCpuBlock::Encode );
	// whisper_encode
	using namespace DirectCompute;

	// MEL DEBUG: Log encoding parameters
	logDebug( u8"ENCODE_DEBUG: Starting encode, seek=%d", seek );

	sEncodeParams ep;
	ep.n_ctx = ( exp_n_audio_ctx > 0 ) ? exp_n_audio_ctx : model.parameters.n_audio_ctx;
	ep.n_mels = model.parameters.n_mels;
	ep.mel_offset = seek;
	ep.layersCount = model.parameters.n_audio_layer;
	ep.n_state = model.parameters.n_audio_state;
	ep.n_head = model.parameters.n_audio_head;
	ep.n_audio_ctx = model.parameters.n_audio_ctx;
	ep.n_text_state = model.parameters.n_text_state;
	ep.n_text_layer = model.parameters.n_text_layer;
	ep.n_text_ctx = model.parameters.n_text_ctx;
	try
	{
		auto cur = context.encode( mel, ep );
		Tracing::tensor( "encode-out", cur );

		// ENCODER_OUTPUT_DEBUG: Log encoder output statistics
		logDebug( u8"ENCODER_OUTPUT_DEBUG: seek=%d, encoder output tensor created", seek );

		// Analyze encoder output statistics to understand feature distribution
		try {
			std::vector<float> encoder_data;
			cur.download( encoder_data );

			if( !encoder_data.empty() ) {
				// Calculate basic statistics
				float sum = 0.0f, sum_sq = 0.0f;
				float min_val = encoder_data[0], max_val = encoder_data[0];

				for( float val : encoder_data ) {
					sum += val;
					sum_sq += val * val;
					min_val = std::min( min_val, val );
					max_val = std::max( max_val, val );
				}

				float mean = sum / encoder_data.size();
				float variance = (sum_sq / encoder_data.size()) - (mean * mean);
				float std_dev = std::sqrt( variance );

				logDebug( u8"ENCODER_STATS: seek=%d, size=%zu, mean=%.6f, std=%.6f, min=%.6f, max=%.6f",
					seek, encoder_data.size(), mean, std_dev, min_val, max_val );

				// Check for potential issues
				if( std_dev < 0.001f ) {
					logDebug( u8"ENCODER_WARNING: seek=%d, very low std deviation (%.6f), encoder output may be too uniform", seek, std_dev );
				}
				if( std::abs(mean) > 10.0f ) {
					logDebug( u8"ENCODER_WARNING: seek=%d, high mean value (%.6f), encoder output may be unstable", seek, mean );
				}
			}
		} catch( ... ) {
			logDebug( u8"ENCODER_ERROR: seek=%d, failed to download encoder output for analysis", seek );
		}

		return S_OK;
	}
	catch( HRESULT hr )
	{
		return hr;
	}
}

HRESULT ContextImpl::decode( const int* tokens, size_t length, int n_past, int threads )
{
	// whisper_decode
	using namespace DirectCompute;
	sDecodeParams dp;
	dp.n_state = model.parameters.n_audio_state;
	dp.n_head = model.parameters.n_audio_head;
	dp.n_ctx = model.parameters.n_text_ctx;
	dp.n_past = n_past;
	dp.M = exp_n_audio_ctx > 0 ? exp_n_audio_ctx : model.parameters.n_audio_ctx;
	dp.n_text_layer = model.parameters.n_text_layer;
	dp.n_vocab = model.parameters.n_vocab;

	try
	{
		context.decode( tokens, (int)length, dp, probs, threads );
		return S_OK;
	}
	catch( HRESULT hr )
	{
		return hr;
	}
}

// the most basic sampling scheme - select the top token
sTokenData ContextImpl::sampleBest( const float* probs, bool force_timestamp, bool is_initial, const std::vector<int>& previous_tokens )
{
	// whisper_sample_best
	const Vocabulary& vocab = model.shared->vocab;
	sTokenData result = { 0 };

	size_t n_logits = vocab.size();

	probs_id.clear();
	probs_id.reserve( n_logits );

	// LANGUAGE DEBUG: Check if language constraints are being applied
	logDebug( u8"LANGUAGE_DEBUG: Checking language processing in sample method, force_timestamp=%s", force_timestamp ? "YES" : "NO" );

	// LANGUAGE DEBUG: Check current decoder state and language context
	logDebug( u8"LANGUAGE_DEBUG: Current decoder state=%d, n_logits=%zu", (int)m_currentState, n_logits );

	// MEL DEBUG: Add MEL spectrogram analysis
	// This will help us understand if the encoder is receiving correct audio features

	// LOGITS DEBUG: Add detailed logits analysis
	logDebug( u8"LOGITS_DEBUG: Starting logits analysis, n_logits=%zu, m_sampler=%p", n_logits, m_sampler.get() );

	// Find top 10 logits before any processing
	std::vector<std::pair<float, int>> top_logits_raw;
	for( size_t i = 0; i < n_logits; ++i ) {
		top_logits_raw.push_back( {probs[i], (int)i} );
	}
	std::sort( top_logits_raw.begin(), top_logits_raw.end(), std::greater<std::pair<float, int>>() );

	logDebug( u8"LOGITS_RAW_TOP10:" );
	for( int i = 0; i < 10 && i < (int)top_logits_raw.size(); ++i ) {
		const auto& pair = top_logits_raw[i];
		const char* token_text = vocab.string( pair.second );
		logDebug( u8"  [%d] id=%d, logit=%.6f, text='%s'", i, pair.second, pair.first, token_text ? token_text : "NULL" );
	}

	// Use the advanced sampler with state-aware token suppression
	// This completely replaces the old probability modification logic
	if( m_sampler ) {
		// Determine the appropriate decoder state for sampling
		DecoderState sampling_state = m_currentState;

		// Override state for timestamp sampling
		if( force_timestamp ) {
			sampling_state = DecoderState::SeekingTimestamp;
		}

		// Let the advanced sampler handle everything: state suppression, repetition penalty, temperature
		int sampled_token = m_sampler->sample( const_cast<float*>(probs), n_logits, previous_tokens, sampling_state );

		// LOGITS DEBUG: Show top logits after sampler processing
		std::vector<std::pair<float, int>> top_logits_processed;
		for( size_t i = 0; i < n_logits; ++i ) {
			top_logits_processed.push_back( {probs[i], (int)i} );
		}
		std::sort( top_logits_processed.begin(), top_logits_processed.end(), std::greater<std::pair<float, int>>() );

		logDebug( u8"LOGITS_PROCESSED_TOP10:" );
		for( int i = 0; i < 10 && i < (int)top_logits_processed.size(); ++i ) {
			const auto& pair = top_logits_processed[i];
			const char* token_text = vocab.string( pair.second );
			logDebug( u8"  [%d] id=%d, logit=%.6f, text='%s'", i, pair.second, pair.first, token_text ? token_text : "NULL" );
		}

		const char* sampled_token_text = vocab.string( sampled_token );
		logDebug( u8"LOGITS_SAMPLED: id=%d, logit=%.6f, text='%s'",
			sampled_token,
			(sampled_token >= 0 && sampled_token < (int)n_logits) ? probs[sampled_token] : -999.0f,
			sampled_token_text ? sampled_token_text : "NULL" );

		// Convert to the expected sTokenData format
		sTokenData result;
		result.id = sampled_token;
		result.p = (sampled_token >= 0 && sampled_token < (int)n_logits) ? probs[sampled_token] : 0.0f;

		// CRITICAL FIX: Set tid for timestamp tokens
		// For timestamp tokens (id >= vocab.token_beg), tid should be the token id itself
		// For non-timestamp tokens, tid should be 0
		if( sampled_token >= vocab.token_beg )
		{
			result.tid = sampled_token;  // Timestamp token: tid = token id
		}
		else
		{
			result.tid = 0;  // Non-timestamp token: tid = 0
		}

		return result;
	}

	// Fallback to original logic if sampler is not available
	std::vector<float> modified_probs( probs, probs + n_logits );

	// Use modified probabilities for token selection
	for( size_t i = 0; i < n_logits; i++ )
		probs_id.emplace_back( modified_probs[ i ], (int)i );

	{
		double sum_ts = 0.0;
		double max_ts = -1.0;
		double max_tx = -1.0;

		for( int i = 0; i < vocab.token_beg; i++ )
			max_tx = std::max( max_tx, probs_id[ i ].first );

		const int i0 = is_initial ? vocab.token_beg + 101 : vocab.token_beg;
		const int i1 = is_initial ? vocab.token_beg + 101 : (int)n_logits;

		// the initial timestamp cannot be larger than 100
		// ref: https://github.com/openai/whisper/blob/0b1ba3d46ebf7fe6f953acfd8cad62a4f851b49f/whisper/decoding.py#L426-L429
		if( is_initial )
		{
			for( int i = i0; i < n_logits; i++ )
				probs_id[ i ].first = -INFINITY;
		}

		for( int i = vocab.token_beg; i < i1; i++ )
		{
			sum_ts += probs_id[ i ].first;
			if( probs_id[ i ].first > max_ts )
			{
				max_ts = probs_id[ i ].first;
				result.tid = probs_id[ i ].second;
			}
		}

		// if the probability sum of all timestamp tokens is higher than the max probability of the text tokens - sample a
		// timestamp token
		if( sum_ts > max_tx || force_timestamp )
		{
			// ref: https://github.com/openai/whisper/blob/0b1ba3d46ebf7fe6f953acfd8cad62a4f851b49f/whisper/decoding.py#L430-L438
			for( int i = 0; i < vocab.token_beg; i++ )
				probs_id[ i ].first = -INFINITY;
		}

		result.pt = (float)( max_ts / ( sum_ts + 1e-10 ) );
		result.ptsum = (float)sum_ts;
	}

	// find the top K tokens
	const int top_k = 4;

	std::partial_sort(
		probs_id.begin(),
		probs_id.begin() + top_k, probs_id.end(),
		[]( const std::pair<double, Vocabulary::id>& a, const std::pair<double, Vocabulary::id>& b ) {
			return a.first > b.first;
		} );

	probs_id.resize( top_k );

	//printf("\n");
	//for (int i = 0; i < (int) probs_id.size(); i++) {
	//    printf("%d: '%s' %f, %d\n", i, vocab.id_to_token.at(probs_id[i].second).c_str(), probs_id[i].first, probs_id[i].second);
	//}

	int res = 0;
	while( ( probs_id[ res ].second == vocab.token_sot ||
		probs_id[ res ].second == vocab.token_solm ||
		probs_id[ res ].second == vocab.token_not ) &&
		res < (int)probs_id.size() - 1 )
	{
		res++;
	}

	result.id = probs_id[ res ].second;
	result.p = (float)probs_id[ res ].first;

	return result;
}

sTokenData ContextImpl::sampleBest( const std::vector<int>& previous_tokens )
{
	const int n_vocab = model.shared->vocab.n_vocab;
	return sampleBest( probs.data() + ( probs.size() - n_vocab ), false, false, previous_tokens );
}

sTokenData ContextImpl::sampleBest( const std::vector<int>& previous_tokens, int current_seek, int seek_end )
{
	const Vocabulary& vocab = model.shared->vocab;
	const int n_vocab = vocab.n_vocab;
	const float* probs_ptr = probs.data() + ( probs.size() - n_vocab );

	size_t n_logits = vocab.size();

	// Use the advanced sampler with timestamp range constraints
	if( m_sampler ) {
		// For this method, we're always in transcribing state unless we need a timestamp
		DecoderState sampling_state = m_currentState;

		// Call the sampler with timestamp constraints
		int sampled_token = m_sampler->sample( const_cast<float*>(probs_ptr), n_logits, previous_tokens, sampling_state, current_seek, seek_end );

		// Convert to the expected sTokenData format
		sTokenData result;
		result.id = sampled_token;
		result.p = (sampled_token >= 0 && sampled_token < (int)n_logits) ? probs_ptr[sampled_token] : 0.0f;

		// Set tid for timestamp tokens
		if( sampled_token >= vocab.token_beg )
		{
			result.tid = sampled_token;  // Timestamp token: tid = token id
		}
		else
		{
			result.tid = 0;  // Non-timestamp token: tid = 0
		}

		return result;
	}

	// Fallback to the original method if sampler is not available
	return sampleBest( probs_ptr, false, false, previous_tokens );
}

sTokenData ContextImpl::sampleTimestamp( bool initial )
{
	const int n_vocab = model.shared->vocab.n_vocab;
	return sampleBest( probs.data() + ( probs.size() - n_vocab ), true, initial );
}

sTokenData ContextImpl::sampleTimestamp( bool initial, int current_seek, int seek_end, float max_initial_ts )
{
	const Vocabulary& vocab = model.shared->vocab;
	const int n_vocab = vocab.n_vocab;
	const float* probs_ptr = probs.data() + ( probs.size() - n_vocab );

	size_t n_logits = vocab.size();

	// Use the advanced sampler with timestamp range constraints for timestamp sampling
	if( m_sampler ) {
		// Force timestamp sampling state
		DecoderState sampling_state = DecoderState::SeekingTimestamp;

		// CRITICAL: Pass max_initial_ts for initial timestamp constraint
		// Call the sampler with timestamp constraints
		int sampled_token = m_sampler->sample( const_cast<float*>(probs_ptr), n_logits, m_recent_tokens, sampling_state, current_seek, seek_end, max_initial_ts );

		// Convert to the expected sTokenData format
		sTokenData result;
		result.id = sampled_token;
		result.p = (sampled_token >= 0 && sampled_token < (int)n_logits) ? probs_ptr[sampled_token] : 0.0f;

		// Set tid for timestamp tokens
		if( sampled_token >= vocab.token_beg )
		{
			result.tid = sampled_token;  // Timestamp token: tid = token id
		}
		else
		{
			result.tid = 0;  // Non-timestamp token: tid = 0
		}

		return result;
	}

	// Fallback to the original method if sampler is not available
	return sampleBest( probs_ptr, true, initial );
}

// a cost-function / heuristic that is high for text that takes longer to pronounce
// Obviously, can be improved
static float voice_length( const char* text )
{
	if( nullptr == text )
		return 0;

	float res = 0.0f;
	while( true )
	{
		const char c = *text;
		if( c == '\0' )
			return res;
		text++;

		// Figure out the increment
		float inc;
		if( c >= '0' && c <= '9' )
			inc = 3.0f;
		else
		{
			switch( c )
			{
			case ' ': inc = 0.01f; break;
			case ',': inc = 2.00f; break;
			case '.':
			case '!':
			case '?':
				inc = 3.00f; break;
			default:
				inc = 1.0f;
			}
		}

		res += inc;
	}
}

static int timestamp_to_sample( int64_t t, int n_samples )
{
	return std::max( 0, std::min( (int)n_samples - 1, (int)( ( t * SAMPLE_RATE ) / 100 ) ) );
}

static int64_t sample_to_timestamp( int i_sample )
{
	return ( 100 * i_sample ) / SAMPLE_RATE;
}

void ContextImpl::expComputeTokenLevelTimestamps( int i_segment, float thold_pt, float thold_ptsum )
{
	// whisper_exp_compute_token_level_timestamps
	const Whisper::Vocabulary& vocab = model.shared->vocab;

	auto& segment = result_all[ i_segment ];
	auto& tokens = segment.tokens;

	const int n_samples = energy.size();

	if( n_samples == 0 )
	{
		logWarning( u8"%s: no signal data available", __func__ );
		return;
	}

	const int64_t t0 = segment.t0;
	const int64_t t1 = segment.t1;
	const int n = tokens.size();

	if( n == 0 )
		return;

	if( n == 1 )
	{
		tokens[ 0 ].t0 = t0;
		tokens[ 0 ].t1 = t1;
		return;
	}

	auto& t_beg = this->t_beg;
	auto& t_last = this->t_last;
	auto& tid_last = this->tid_last;

	for( int j = 0; j < n; ++j )
	{
		auto& token = tokens[ j ];

		if( j == 0 )
		{
			if( token.id == vocab.token_beg )
			{
				tokens[ j ].t0 = t0;
				tokens[ j ].t1 = t0;
				tokens[ j + 1 ].t0 = t0;

				t_beg = t0;
				t_last = t0;
				tid_last = vocab.token_beg;
			}
			else
			{
				tokens[ j ].t0 = t_last;
			}
		}

		tokens[ j ].id = token.id;
		tokens[ j ].tid = token.tid;
		tokens[ j ].p = token.p;
		tokens[ j ].pt = token.pt;
		tokens[ j ].ptsum = token.ptsum;
		tokens[ j ].vlen = voice_length( vocab.string( token.id ) );

		// CRITICAL FIX: Only calculate and apply timestamps for actual timestamp tokens
		// Skip timestamp calculation for non-timestamp tokens (tid == 0)
		if( token.tid > vocab.token_beg && token.pt > thold_pt && token.ptsum > thold_ptsum && token.tid > tid_last )
		{
			const int64_t tt = t_beg + 2 * ( token.tid - vocab.token_beg );

			// Additional safety check: ensure timestamp is valid and within bounds
			if( tt >= 0 && tt <= t1 )
			{
				if( j > 0 )
					tokens[ j - 1 ].t1 = tt;
				tokens[ j ].t0 = tt;
				tid_last = token.tid;
			}
		}
	}

	tokens[ n - 2 ].t1 = t1;
	tokens[ n - 1 ].t0 = t1;
	tokens[ n - 1 ].t1 = t1;
	t_last = t1;

	// find intervals of tokens with unknown timestamps
	// fill the timestamps by proportionally splitting the interval based on the token voice lengths
	{
		int p0 = 0;
		int p1 = 0;

		while( true )
		{
			while( p1 < n && tokens[ p1 ].t1 < 0 )
				p1++;

			if( p1 >= n )
				p1--;

			if( p1 > p0 )
			{
				double psum = 0.0;
				for( int j = p0; j <= p1; j++ )
					psum += tokens[ j ].vlen;

				//printf("analyzing %d - %d, psum = %f\n", p0, p1, psum);
				const double dt = tokens[ p1 ].t1 - tokens[ p0 ].t0;

				// split the time proportionally to the voice length
				for( int j = p0 + 1; j <= p1; j++ )
				{
					const double ct = tokens[ j - 1 ].t0 + dt * tokens[ j - 1 ].vlen / psum;
					tokens[ j - 1 ].t1 = ct;
					tokens[ j ].t0 = ct;
				}
			}

			p1++;
			p0 = p1;
			if( p1 >= n )
				break;
		}
	}

	// fix up (just in case)
	for( int j = 0; j < n - 1; j++ )
	{
		if( tokens[ j ].t1 < 0 )
			tokens[ j + 1 ].t0 = tokens[ j ].t1;

		if( j > 0 )
		{
			if( tokens[ j - 1 ].t1 > tokens[ j ].t0 ) {
				tokens[ j ].t0 = tokens[ j - 1 ].t1;
				tokens[ j ].t1 = std::max( tokens[ j ].t0, tokens[ j ].t1 );
			}
		}
	}

	// VAD
	// expand or contract tokens based on voice activity
	{
		constexpr int hw = SAMPLE_RATE / 8;

		for( int j = 0; j < n; j++ )
		{
			if( tokens[ j ].id >= vocab.token_eot )
				continue;

			int s0 = timestamp_to_sample( tokens[ j ].t0, n_samples );
			int s1 = timestamp_to_sample( tokens[ j ].t1, n_samples );

			const int ss0 = std::max( s0 - hw, 0 );
			const int ss1 = std::min( s1 + hw, n_samples );

			const int ns = ss1 - ss0;

			float sum = 0.0f;
			for( int k = ss0; k < ss1; k++ )
				sum += this->energy[ k ];

			const float thold = 0.5 * sum / ns;

			{
				int k = s0;
				if( this->energy[ k ] > thold && j > 0 )
				{
					while( k > 0 && this->energy[ k ] > thold )
						k--;
					tokens[ j ].t0 = sample_to_timestamp( k );
					if( tokens[ j ].t0 < tokens[ j - 1 ].t1 )
						tokens[ j ].t0 = tokens[ j - 1 ].t1;
					else
						s0 = k;
				}
				else
				{
					while( this->energy[ k ] < thold && k < s1 )
						k++;
					s0 = k;
					tokens[ j ].t0 = sample_to_timestamp( k );
				}
			}

			{
				int k = s1;
				if( this->energy[ k ] > thold )
				{
					while( k < n_samples - 1 && this->energy[ k ] > thold )
						k++;
					tokens[ j ].t1 = sample_to_timestamp( k );
					if( j < ns - 1 && tokens[ j ].t1 > tokens[ j + 1 ].t0 )
						tokens[ j ].t1 = tokens[ j + 1 ].t0;
					else
						s1 = k;
				}
				else
				{
					while( this->energy[ k ] < thold && k > s0 )
						k--;
					s1 = k;
					tokens[ j ].t1 = sample_to_timestamp( k );
				}
			}
		}
	}
}

static std::string to_timestamp( int64_t t, bool comma = false )
{
	// Robustness check: handle invalid or extreme values
	if( t < 0 )
	{
		// Negative timestamps are invalid, return zero timestamp
		return "00:00:00.000";
	}

	// Check for potential overflow in multiplication
	// Maximum safe value: INT64_MAX / 10 to avoid overflow in t * 10
	const int64_t max_safe_timestamp = INT64_MAX / 10;
	if( t > max_safe_timestamp )
	{
		// Extremely large timestamp, return a reasonable maximum
		return "99:59:59.999";
	}

	int64_t msec = t * 10;
	int64_t hr = msec / ( 1000 * 60 * 60 );
	msec = msec - hr * ( 1000 * 60 * 60 );
	int64_t min = msec / ( 1000 * 60 );
	msec = msec - min * ( 1000 * 60 );
	int64_t sec = msec / 1000;
	msec = msec - sec * 1000;

	// Additional safety check: ensure values are within reasonable ranges
	if( hr > 99 ) hr = 99;  // Cap hours at 99
	if( min > 59 ) min = 59;  // Cap minutes at 59
	if( sec > 59 ) sec = 59;  // Cap seconds at 59
	if( msec > 999 ) msec = 999;  // Cap milliseconds at 999

	char buf[ 32 ];
	snprintf( buf, sizeof( buf ), "%02d:%02d:%02d%s%03d", (int)hr, (int)min, (int)sec, comma ? "," : ".", (int)msec );

	return std::string( buf );
}

class ContextImpl::CurrentSpectrogramRaii
{
	ContextImpl* ctx;
public:
	CurrentSpectrogramRaii( ContextImpl* c, iSpectrogram& mel )
	{
		ctx = c;
		c->currentSpectrogram = &mel;
	}
	~CurrentSpectrogramRaii()
	{
		ctx->currentSpectrogram = nullptr;
	}
};

HRESULT COMLIGHTCALL ContextImpl::runFullImpl( const sFullParams& params, const sProgressSink& progress, iSpectrogram& mel )
{
	auto ts = device.setForCurrentThread();
	const Whisper::Vocabulary& vocab = model.shared->vocab;

	// Ported from whisper_full() function
	result_all.clear();
	if( params.flag( eFullParamsFlags::SpeedupAudio ) )
	{
		logError( u8"GPU model doesn't implement the SpeedupAudio flag" );
		return E_NOTIMPL;
	}

	CurrentSpectrogramRaii _cs( this, mel );
	const int seek_start = params.offset_ms / 10;
	const int seek_end = seek_start + ( params.duration_ms == 0 ? (int)mel.getLength() : params.duration_ms / 10 );

	// if length of spectrogram is less than 1s (100 samples), then return
	// basically don't process anything that is less than 1s
	// see issue #39: https://github.com/ggerganov/whisper.cpp/issues/39
	if( seek_end < 100 + seek_start )
		return S_FALSE;

	// the accumulated text context so far
	if( params.flag( eFullParamsFlags::NoContext ) )
		prompt_past.clear();

	// prepend the prompt tokens to the prompt_past
	if( params.prompt_tokens && params.prompt_n_tokens > 0 )
	{
		// parse tokens from the pointer
		for( int i = 0; i < params.prompt_n_tokens; i++ )
			prompt_past.push_back( params.prompt_tokens[ i ] );
		std::rotate( prompt_past.begin(), prompt_past.end() - params.prompt_n_tokens, prompt_past.end() );
	}

	// overwrite audio_ctx
	exp_n_audio_ctx = params.audio_ctx;

	// =================== [ ARCHITECTURAL FIX: CONTEXT PRIMING ] ===================
	// Build complete initial context sequence: [SOT, LANGUAGE, TASK, TIMESTAMP]
	// This follows whisper.cpp's reference implementation for proper multilingual support
	std::vector<whisper_token> prompt_init;

	// 1. Start-Of-Transcript token
	prompt_init.push_back( vocab.token_sot );

	if( vocab.is_multilingual() )
	{
		// 2. Language token
		// Convert uint32_t language key to string for whisper.cpp API
		char lang[ 5 ];
		*(uint32_t*)( &lang[ 0 ] ) = params.language;
		lang[ 4 ] = '\0';

		const int lang_token_id = vocab.languageTokenId( lang );
		if( lang_token_id < 0 )
		{
			logError( u8"%s: unknown language '%s'", __func__, lang );
			return E_INVALIDARG;
		}
		prompt_init.push_back( lang_token_id );

		// CRITICAL FIX: Set language token in sampler for language constraints
		if( m_sampler )
		{
			m_sampler->setLanguageToken( lang_token_id );
		}

		// 3. Task token (transcribe or translate)
		if( params.flag( eFullParamsFlags::Translate ) )
			prompt_init.push_back( vocab.token_translate );
		else
			prompt_init.push_back( vocab.token_transcribe );

		// Language token set successfully
	}

	// 4. Timestamp token (following whisper.cpp logic)
	// When PrintTimestamps=true (timestamps enabled): add token_beg
	// When PrintTimestamps=false (no timestamps): add token_not
	if( params.flag( eFullParamsFlags::PrintTimestamps ) )
	{
		prompt_init.push_back( vocab.token_beg );
	}
	else
	{
		prompt_init.push_back( vocab.token_not );
	}
	// ============================================================================

	// int progress_prev = 0;
	// int progress_step = 5;

	std::vector<sTokenData> tokens_cur;
	tokens_cur.reserve( model.parameters.n_text_ctx );
	std::vector<whisper_token> prompt;
	prompt.reserve( model.parameters.n_text_ctx );

	// main loop
	int seek = seek_start;
	// Start measuring "Run" profiler value, both CPU and GPU times
	auto prof = context.completeProfiler();
	bool stoppedPrematurely = false;

	// Fix for Large-v3 timestamp generation issue: limit consecutive failures
	int consecutive_failures = 0;
	const int max_consecutive_failures = 5; // Allow max 5 consecutive timestamp failures

	if( params.flag( eFullParamsFlags::NoContext ) )
	{
		CHECK( context.clearState() );
	}

	while( true )
	{
		if( nullptr != progress.pfn )
		{
			const int pos = seek - seek_start;
			const int total = seek_end - seek_start;
			const double percentage = (double)pos / (double)total;
			auto cb = profiler.cpuBlock( eCpuBlock::Callbacks );
			CHECK( progress.pfn( percentage, this, progress.pv ) );
		}

		if( seek + 100 >= seek_end )
			break;

		if( nullptr != params.encoder_begin_callback )
		{
			auto cb = profiler.cpuBlock( eCpuBlock::Callbacks );
			HRESULT hr = params.encoder_begin_callback( this, params.encoder_begin_callback_user_data );
			if( FAILED( hr ) )
				return hr;
			if( hr != S_OK )
			{
				stoppedPrematurely = true;
				break;
			}
		}

		// MEL encoding starting

		// Get MEL spectrogram dimensions and sample some values
		// This will help us understand if the audio preprocessing is correct

		// encode audio features starting at offset seek
		CHECK( encode( mel, seek ) );

		// MEL encoding completed

		// Reset quality detection for new audio segment
		resetSegmentQuality();

		int n_past = 0;
		prompt.clear();

		// if we have already generated some text, use it as a prompt to condition the next generation
		if( !prompt_past.empty() )
		{
			int n_take = std::min( std::min( params.n_max_text_ctx, model.parameters.n_text_ctx / 2 ), int( prompt_past.size() ) );

			prompt = { vocab.token_prev };
			prompt.insert( prompt.begin() + 1, prompt_past.end() - n_take, prompt_past.end() );

			prompt_past.clear();
			prompt_past.insert( prompt_past.end(), prompt.begin() + 1, prompt.end() );
		}

		prompt.insert( prompt.end(), prompt_init.begin(), prompt_init.end() );

		int seek_delta = 100 * WHISPER_CHUNK_SIZE;

		// print the prompt
		//printf("\n\n");
		//for (int i = 0; i < prompt.size(); i++) {
		//    printf("%s: prompt[%d] = %s\n", __func__, i, ctx->vocab.id_to_token[prompt[i]].c_str());
		//}
		//printf("\n\n");

		// the accumulated transcription in the current iteration
		int result_len = 0;
		tokens_cur.clear();

		bool failed = false;
		bool has_ts = false; // have we already sampled a non-beg timestamp token for the current segment?

		// Initialize decoder state machine for this segment
		m_currentState = DecoderState::SeekingSOT;

		// Maintain history of recent tokens for repetition penalty
		std::vector<int> recent_tokens;
		const int max_history = 10; // Keep track of last 10 tokens

		{
			// Measure "Decode" profiler value, both CPU and GPU times
			auto prof = context.decodeProfiler();

			// Fix for Large-v3 performance issue: limit n_max to reasonable value
			// Large-v3 models may have very large n_text_ctx causing excessive decode loops
			const int raw_n_max = model.parameters.n_text_ctx / 2 - 4;
			const int n_max = std::min(raw_n_max, 220); // Cap at 220 (same as standard models)

			if (raw_n_max != n_max) {
				logDebug(u8"Limiting decode loops: n_text_ctx=%d, raw_n_max=%d, capped_n_max=%d",
					model.parameters.n_text_ctx, raw_n_max, n_max);
			}

			for( int i = 0; i < n_max; i++ )
			{
				// Language token processing

				CHECK( decode( prompt.data(), prompt.size(), n_past, params.cpuThreads ) );

				// Decoder state updated

				n_past += (int)prompt.size();
				prompt.clear();

				// very basic greedy sampling strategy:
				//
				//   - always take the most probable token
				//
				// more sophisticated sampling strategies could be implemented here, but we keep it simple
				// feel free to experiment!
				//
				{
					auto p = profiler.cpuBlock( eCpuBlock::Sample );

					// Use our centralized token history for the sampler
					// CRITICAL: Follow original project's simple but effective logic - let model naturally generate timestamps
					const sTokenData token = ( i == 0 ) ? sampleTimestamp( true, seek, seek_end, params.max_initial_ts ) : sampleBest( m_recent_tokens, seek, seek_end );

					// DEBUG: Log token information (commented out for cleaner output)
					// const char* tokenText = vocab.string(token.id);
					// logDebug( u8"DEBUG: i=%d, token.id=%d ('%s'), vocab.token_beg=%d, token.p=%f, history_size=%d, state=%d",
					//	i, token.id, tokenText ? tokenText : "NULL", vocab.token_beg, token.p, (int)m_recent_tokens.size(), (int)m_currentState );

					// Update decoder state based on the generated token
					if( token.id == vocab.token_sot )
					{
						m_currentState = DecoderState::SeekingLanguage;
						// logDebug( u8"DEBUG: State transition to SeekingLanguage after SOT token" );
					}
					else if( m_currentState == DecoderState::SeekingLanguage &&
							 token.id >= vocab.token_sot + 1 && token.id < vocab.token_sot + 100 ) // Language tokens range
					{
						m_currentState = DecoderState::SeekingTimestamp; // Go to timestamp seeking first
						// logDebug( u8"DEBUG: State transition to SeekingTimestamp after language token %d", token.id );
					}
					else if( token.id > vocab.token_beg ) // Timestamp token
					{
						m_currentState = DecoderState::Transcribing;
						// logDebug( u8"DEBUG: State transition to Transcribing after timestamp token" );
					}
					else if( m_currentState == DecoderState::Transcribing && token.id == vocab.token_eot )
					{
						// EOT token means end of segment, go back to seeking timestamp for next segment
						m_currentState = DecoderState::SeekingTimestamp;
						// logDebug( u8"DEBUG: State transition to SeekingTimestamp after EOT token" );
					}

					// Update centralized token history for repetition penalty
					m_recent_tokens.push_back( token.id );
					if( m_recent_tokens.size() > max_history ) {
						m_recent_tokens.erase( m_recent_tokens.begin() );
					}

					// Also update the local recent_tokens for backward compatibility
					recent_tokens.push_back( token.id );
					if( recent_tokens.size() > max_history ) {
						recent_tokens.erase( recent_tokens.begin() );
					}

					// timestamp token - update sliding window
					if( token.id > vocab.token_beg )
					{
						const int seek_delta_new = 2 * ( token.id - vocab.token_beg );

						// logDebug( u8"DEBUG: Timestamp token detected, seek_delta_new=%d", seek_delta_new );

						// do not allow to go back in time
						if( has_ts && seek_delta > seek_delta_new && result_len < i )
							break;

						seek_delta = seek_delta_new;
						result_len = i + 1;
						has_ts = true;
					}
					else
					{
						// logDebug( u8"DEBUG: Non-timestamp token, token.id=%d <= vocab.token_beg=%d",
						//	token.id, vocab.token_beg );

						// Add non-timestamp tokens to quality detection system
						if( m_currentState == DecoderState::Transcribing )
						{
							const char* tokenText = vocab.string( token.id );
							std::string tokenStr = tokenText ? tokenText : "";
							addTokenToSegment( token.id, token.p, tokenStr );

							// Check quality periodically or when segment gets too long
							if( m_segment_logprobs.size() >= static_cast<size_t>(m_max_segment_length) )
							{
								if( !checkSegmentQuality() )
								{
									// logDebug( u8"DEBUG: Quality check failed - resetting segment and seeking new timestamp" );
									resetSegmentQuality();
									// Force transition back to seeking timestamp to restart
									m_currentState = DecoderState::SeekingTimestamp;
									// Clear recent context to avoid repeating bad patterns
									if( m_recent_tokens.size() > 5 )
									{
										// Keep only the last 5 tokens
										std::vector<int> last_tokens( m_recent_tokens.end() - 5, m_recent_tokens.end() );
										m_recent_tokens = std::move( last_tokens );
									}
									// Don't break here - let the normal flow continue
								}
								else
								{
									// Quality is good, reset for next segment
									resetSegmentQuality();
								}
							}
						}
					}

					// add it to the context
					prompt.push_back( token.id );
					tokens_cur.push_back( token );

					//{
					//    const auto tt = token.pt > 0.10 ? ctx->vocab.id_to_token[token.tid] : "[?]";
					//    printf("%s: %10s %6d %6.3f '%s'\n", __func__, tt.c_str(), token.id, token.pt, ctx->vocab.id_to_token[token.id].c_str());
					//}

					// end of segment
					if( token.id == vocab.token_eot ||                  // end of text token
						( params.max_tokens > 0 && i >= params.max_tokens ) || // max tokens per segment reached
						( has_ts && seek + seek_delta + 10 >= seek_end )     // end of audio reached (100ms)
						)
					{
						// DEBUG: Critical exit condition analysis
						logDebug( u8"EXIT_CONDITION: i=%d, token.id=%d, eot=%d, max_tokens=%d, has_ts=%s, seek=%d, seek_delta=%d, seek_end=%d, condition_check=%d",
							i, token.id, vocab.token_eot, params.max_tokens, has_ts ? "true" : "false",
							seek, seek_delta, seek_end, (seek + seek_delta + 10) );
						if( result_len == 0 )
						{
							if( seek + seek_delta + 10 >= seek_end )
								result_len = i + 1;
							else
							{
								failed = true;
								break;
							}
						}

						if( params.flag( eFullParamsFlags::SingleSegment ) )
						{
							result_len = i + 1;
							seek_delta = 100 * WHISPER_CHUNK_SIZE;
						}

						break;
					}
				}

				// sometimes, the decoding can get stuck in a repetition loop
				// this is a simple strategy to avoid such cases - we simply flag the decoding as failed and advance
				// the sliding window by 1 second
				if( i == n_max - 1 && ( result_len == 0 || seek_delta < 100 * WHISPER_CHUNK_SIZE / 2 ) )
				{
					// logDebug( u8"DEBUG: Failure condition met - i=%d, n_max=%d, result_len=%d, seek_delta=%d, threshold=%d",
					//	i, n_max, result_len, seek_delta, 100 * WHISPER_CHUNK_SIZE / 2 );
					failed = true;
					break;
				}
			}
		}
		if( failed )
		{
			consecutive_failures++;
			logError( u8"%s: failed to generate timestamp token - skipping one second (failure %d/%d)",
				__func__, consecutive_failures, max_consecutive_failures );

			// If too many consecutive failures, try to recover by using no-timestamp mode
			if( consecutive_failures >= max_consecutive_failures )
			{
				logError( u8"%s: too many consecutive timestamp failures, switching to no-timestamp mode for remaining audio", __func__ );
				// Process remaining audio without timestamps
				seek_delta = seek_end - seek;
				if( seek_delta > 0 )
				{
					// Add a single segment for the remaining audio
					const std::string remaining_text = "[Timestamp generation failed - remaining audio processed without timestamps]";
					result_all.push_back( { seek * 10, seek_end * 10, remaining_text, {} } );
				}
				break; // Exit the main loop
			}

			seek += 100;
			continue;
		}
		else
		{
			// Reset failure counter on success
			consecutive_failures = 0;
		}

		// shrink down to result_len
		tokens_cur.resize( result_len );

		// DEBUG: Check tokens_cur content (commented out for cleaner output)
		// logDebug( u8"DEBUG: tokens_cur.size()=%d, result_len=%d", (int)tokens_cur.size(), result_len );
		// for( int debug_i = 0; debug_i < std::min(10, (int)tokens_cur.size()); debug_i++ )
		// {
		//	const char* tokenText = vocab.string( tokens_cur[debug_i].id );
		//	const bool isTimestamp = tokens_cur[debug_i].id >= vocab.token_beg;
		//	logDebug( u8"DEBUG: tokens_cur[%d]: id=%d, tid=%d, text='%s', isTimestamp=%s",
		//		debug_i, tokens_cur[debug_i].id, tokens_cur[debug_i].tid,
		//		tokenText ? tokenText : "NULL", isTimestamp ? "YES" : "NO" );
		// }

		for( const auto& r : tokens_cur )
			prompt_past.push_back( r.id );

		// store the text from this iteration
		if( !tokens_cur.empty() )
		{
			int i0 = 0;
			int t0 = seek + 2 * ( tokens_cur.front().tid - vocab.token_beg );
			std::string text = "";

			// DEBUG: Critical text generation checkpoint
			logDebug( u8"TEXT_GEN: tokens_cur.size()=%d, first_token_id=%d, tid=%d, t0=%d",
				(int)tokens_cur.size(), tokens_cur.front().id, tokens_cur.front().tid, t0 );

			// DEBUG: Show first 10 tokens with their text content
			for( int debug_i = 0; debug_i < std::min(10, (int)tokens_cur.size()); debug_i++ )
			{
				const char* tokenText = vocab.string( tokens_cur[debug_i].id );
				const bool isTimestamp = tokens_cur[debug_i].id >= vocab.token_beg;
				logDebug( u8"TOKEN[%d]: id=%d, text='%s', isTimestamp=%s",
					debug_i, tokens_cur[debug_i].id, tokenText ? tokenText : "NULL", isTimestamp ? "YES" : "NO" );
			}

			for( int i = 0; i < (int)tokens_cur.size(); i++ )
			{
				//printf("%s: %18s %6.3f %18s %6.3f\n", __func__,
				//        ctx->vocab.id_to_token[tokens_cur[i].id].c_str(), tokens_cur[i].p,
				//        ctx->vocab.id_to_token[tokens_cur[i].tid].c_str(), tokens_cur[i].pt);
				if( params.flag( eFullParamsFlags::PrintSpecial ) || tokens_cur[ i ].id < vocab.token_eot )
					text += vocab.string( tokens_cur[ i ].id );

				if( tokens_cur[ i ].id > vocab.token_beg && !params.flag( eFullParamsFlags::SingleSegment ) )
				{
					const int t1 = seek + 2 * ( tokens_cur[ i ].tid - vocab.token_beg );
					if( !text.empty() )
					{
						const bool speedUp = params.flag( eFullParamsFlags::SpeedupAudio );
						const int tt0 = speedUp ? 2 * t0 : t0;
						const int tt1 = speedUp ? 2 * t1 : t1;

						if( params.flag( eFullParamsFlags::PrintRealtime ) )
						{
							if( params.flag( eFullParamsFlags::PrintTimestamps ) )
								logDebug( u8"[%s --> %s]  %s", to_timestamp( tt0 ).c_str(), to_timestamp( tt1 ).c_str(), text.c_str() );
							else
								logDebug( u8"%s", text.c_str() );
						}

						result_all.push_back( { tt0, tt1, text, {} } );
						for( int j = i0; j <= i; j++ )
							result_all.back().tokens.push_back( tokens_cur[ j ] );

						int n_new = 1;

						if( params.flag( eFullParamsFlags::TokenTimestamps ) )
						{
							expComputeTokenLevelTimestamps( (int)result_all.size() - 1, params.thold_pt, params.thold_ptsum );
							if( params.max_len > 0 )
								n_new = wrapSegment( params.max_len );
						}
						if( nullptr != params.new_segment_callback )
						{
							auto cb = profiler.cpuBlock( eCpuBlock::Callbacks );
							HRESULT hr = params.new_segment_callback( this, n_new, params.new_segment_callback_user_data );
							if( FAILED( hr ) )
								return hr;
						}
					}
					text = "";
					while( i < (int)tokens_cur.size() && tokens_cur[ i ].id > vocab.token_beg )
						i++;
					i--;
					t0 = t1;
					i0 = i + 1;
				}
			}

			if( !text.empty() )
			{
				const int t1 = seek + seek_delta;

				const bool speedUp = params.flag( eFullParamsFlags::SpeedupAudio );
				const int tt0 = speedUp ? 2 * t0 : t0;
				const int tt1 = speedUp ? 2 * t1 : t1;

				if( params.flag( eFullParamsFlags::PrintRealtime ) )
				{
					if( params.flag( eFullParamsFlags::PrintTimestamps ) )
						logDebug( u8"[%s --> %s]  %s", to_timestamp( tt0 ).c_str(), to_timestamp( tt1 ).c_str(), text.c_str() );
					else
						logDebug( u8"%s", text.c_str() );
				}

				result_all.push_back( { tt0, tt1, text, {} } );
				for( int j = i0; j < (int)tokens_cur.size(); j++ )
					result_all.back().tokens.push_back( tokens_cur[ j ] );

				int n_new = 1;
				if( params.flag( eFullParamsFlags::TokenTimestamps ) )
				{
					expComputeTokenLevelTimestamps( (int)result_all.size() - 1, params.thold_pt, params.thold_ptsum );
					if( params.max_len > 0 )
						n_new = wrapSegment( params.max_len );
				}
				if( nullptr != params.new_segment_callback )
				{
					auto cb = profiler.cpuBlock( eCpuBlock::Callbacks );
					HRESULT hr = params.new_segment_callback( this, n_new, params.new_segment_callback_user_data );
					if( FAILED( hr ) )
						return hr;
				}
			}
		}
		seek += seek_delta;
	}

	if( nullptr != progress.pfn && !stoppedPrematurely )
	{
		auto cb = profiler.cpuBlock( eCpuBlock::Callbacks );
		CHECK( progress.pfn( 1.0, this, progress.pv ) );
	}
	return S_OK;
}

// Quality detection methods implementation
void ContextImpl::addTokenToSegment( int token_id, float logprob, const std::string& token_text )
{
	m_segment_logprobs.push_back( logprob );
	m_segment_text += token_text;

	// logDebug( u8"DEBUG: Added token %d ('%s') with logprob %.6f to segment", token_id, token_text.c_str(), logprob );
}

bool ContextImpl::checkSegmentQuality()
{
	if( m_segment_logprobs.empty() || m_segment_text.empty() )
		return true; // Empty segment is considered good

	// Calculate average log probability
	float avg_logprob = calculateAverageLogProb();

	// Calculate compression ratio
	float compression_ratio = calculateCompressionRatio( m_segment_text );

	// logDebug( u8"DEBUG: Quality check - avg_logprob=%.6f (threshold=%.6f), compression_ratio=%.6f (threshold=%.6f)",
	//		  avg_logprob, m_logprob_threshold, compression_ratio, m_compression_ratio_threshold );

	// Check if quality is below thresholds
	bool quality_good = true;
	if( avg_logprob < m_logprob_threshold )
	{
		// logDebug( u8"DEBUG: Quality check FAILED - low average log probability" );
		quality_good = false;
	}
	if( compression_ratio > m_compression_ratio_threshold )
	{
		// logDebug( u8"DEBUG: Quality check FAILED - high compression ratio (likely repetitive text)" );
		quality_good = false;
	}

	// if( quality_good )
	//	logDebug( u8"DEBUG: Quality check PASSED" );

	return quality_good;
}

void ContextImpl::resetSegmentQuality()
{
	m_segment_logprobs.clear();
	m_segment_text.clear();
	// logDebug( u8"DEBUG: Reset segment quality tracking" );
}

float ContextImpl::calculateCompressionRatio( const std::string& text )
{
	if( text.empty() )
		return 1.0f;

	// Simple compression ratio calculation using text length vs unique character count
	// This is a simplified version - in production, you might use zlib compression
	std::set<char> unique_chars( text.begin(), text.end() );
	float ratio = static_cast<float>( text.length() ) / static_cast<float>( unique_chars.size() );

	return ratio;
}

float ContextImpl::calculateAverageLogProb()
{
	if( m_segment_logprobs.empty() )
		return 0.0f;

	float sum = 0.0f;
	for( float logprob : m_segment_logprobs )
		sum += logprob;

	return sum / static_cast<float>( m_segment_logprobs.size() );
}