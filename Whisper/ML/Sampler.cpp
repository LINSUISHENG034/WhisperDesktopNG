#include "stdafx.h"
#include "Sampler.h"
#include "../Whisper/Vocabulary.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <map>
#include <vector>
#include <utility>
#include <functional>

namespace Whisper
{
	WhisperSampler::WhisperSampler(const SamplingParams& params, const Vocabulary& vocab)
		: m_params(params), m_vocab(vocab), m_current_language_token(-1)
	{
	}

	WhisperSampler::~WhisperSampler()
	{
	}

	int WhisperSampler::sample(float* logits, size_t logits_size, const std::vector<int>& history_tokens, DecoderState state)
	{
		// Call the extended version with no timestamp constraints
		return sample(logits, logits_size, history_tokens, state, -1, -1);
	}

	int WhisperSampler::sample(float* logits, size_t logits_size, const std::vector<int>& history_tokens, DecoderState state, int current_seek, int seek_end, float max_initial_ts)
	{
		if (!logits || logits_size == 0)
		{
			return 0; // Return safe default
		}

		// 1. Apply state-based token suppression (MOST IMPORTANT - do this first!)
		suppress_tokens(logits, logits_size, state);

		// 2. Apply timestamp range constraints for SeekingTimestamp state
		if (state == DecoderState::SeekingTimestamp && current_seek >= 0 && seek_end >= 0)
		{
			suppress_out_of_range_timestamps(logits, logits_size, current_seek, seek_end);
		}

		// 3. CRITICAL: Apply max_initial_ts constraint for initial timestamp sampling
		if (state == DecoderState::SeekingTimestamp && current_seek == 0 && max_initial_ts > 0.0f)
		{
			suppress_initial_timestamp(logits, logits_size, max_initial_ts);
		}

		// 4. CRITICAL: Dynamic timestamp sampling COMPLETELY DISABLED
		//
		// DISCOVERY: Both our conservative and original project's simple logic fail in our environment
		// - Conservative: (sum_ts > 0.10) && (sum_ts > max_tx * 100.0) -> Never triggers
		// - Original: (sum_ts > max_tx) -> Always triggers, causes infinite loops
		//
		// ROOT CAUSE: Our environment has different probability distributions than original project
		// - Our sum_ts ≈ 0.029 (2.9%), max_tx ≈ 0.00002 (0.002%)
		// - Ratio ≈ 1400:1, but sum_ts is still low absolute value
		//
		// SOLUTION: Let the model generate naturally without forced timestamp sampling
		// The original project's segmentation relies on natural token_eot generation
		// We should focus on fixing the root cause rather than forcing timestamps

		// 3. Apply language constraints during transcription (NEW - CRITICAL FIX)
		if (state == DecoderState::Transcribing && m_current_language_token != -1)
		{
			apply_language_constraints(logits, logits_size, m_current_language_token);
		}

		// 4. Apply adaptive repetition penalty
		apply_repetition_penalty(logits, logits_size, history_tokens);

		// 5. Apply temperature scaling
		apply_temperature(logits, logits_size);

		// 5. Find the best token directly (greedy sampling)
		// Note: Top-K sampling disabled temporarily due to bug with tiny models
		int best_token_id = 0;
		float max_logit = -FLT_MAX;
		for (size_t i = 0; i < logits_size; ++i)
		{
			if (logits[i] > max_logit)
			{
				max_logit = logits[i];
				best_token_id = static_cast<int>(i);
			}
		}

		return best_token_id;
	}

	// Backward compatibility method - defaults to Transcribing state
	int WhisperSampler::sample(float* logits, size_t logits_size, const std::vector<int>& history_tokens)
	{
		return sample(logits, logits_size, history_tokens, DecoderState::Transcribing);
	}



	void WhisperSampler::updateParams(const SamplingParams& params)
	{
		m_params = params;
	}

	const SamplingParams& WhisperSampler::getParams() const
	{
		return m_params;
	}

	void WhisperSampler::setLanguageToken(int language_token)
	{
		m_current_language_token = language_token;
	}

	void WhisperSampler::apply_repetition_penalty(float* logits, size_t logits_size, const std::vector<int>& history_tokens)
	{
		if (history_tokens.empty())
		{
			return;
		}

		// Use a map to count occurrences of each token in the history window
		std::map<int, int> counts;
		size_t start_index = (history_tokens.size() > static_cast<size_t>(m_params.history_size)) ?
			(history_tokens.size() - static_cast<size_t>(m_params.history_size)) : 0;

		for (size_t i = start_index; i < history_tokens.size(); ++i)
		{
			counts[history_tokens[i]]++;
		}

		for (const auto& [token_id, count] : counts)
		{
			// Validate token ID
			if (!is_valid_token(token_id, logits_size))
			{
				continue;
			}

			// Skip special tokens (timestamps, EOT, etc.) from repetition penalty
			// but NOT from sampling - they should still be available for selection
			if (m_vocab.isSpecial(token_id))
			{
				continue; // Skip special tokens from repetition penalty only
			}

			// Core logic: Adaptive penalty that grows exponentially with repetition count
			// Base penalty of 0.5, exponentially increasing with each repetition
			float penalty = 0.5f * powf(1.5f, static_cast<float>(count - 1));
			logits[token_id] -= penalty;
		}
	}

	void WhisperSampler::apply_temperature(float* logits, size_t logits_size)
	{
		// Skip if temperature is 0 (greedy) or 1 (no change)
		if (m_params.temperature == 0.0f || m_params.temperature == 1.0f)
		{
			return;
		}

		// Apply temperature scaling
		for (size_t i = 0; i < logits_size; ++i)
		{
			logits[i] /= m_params.temperature;
		}
	}

	int WhisperSampler::greedy_sample(const float* logits, size_t logits_size)
	{
		int best_token_id = 0;
		float max_logit = -FLT_MAX;

		for (size_t i = 0; i < logits_size; ++i)
		{
			if (logits[i] > max_logit)
			{
				max_logit = logits[i];
				best_token_id = static_cast<int>(i);
			}
		}

		return best_token_id;
	}

	bool WhisperSampler::is_valid_token(int token_id, size_t logits_size) const
	{
		return token_id >= 0 && static_cast<size_t>(token_id) < logits_size;
	}

	void WhisperSampler::suppress_tokens(float* logits, size_t logits_size, DecoderState state)
	{
		// This is the CORE logic that prevents illegal token selections based on decoder state
		// Reference: whisper.cpp's token suppression logic

		switch (state)
		{
			case DecoderState::SeekingSOT:
				// When seeking SOT, only allow SOT token
				for (size_t i = 0; i < logits_size; ++i)
				{
					if (static_cast<int>(i) != m_vocab.token_sot)
					{
						logits[i] = -FLT_MAX;
					}
				}
				break;

			case DecoderState::SeekingLanguage:
				// When seeking language, only allow language tokens
				// Language tokens are typically in range [token_sot + 1, token_sot + 100]
				for (size_t i = 0; i < logits_size; ++i)
				{
					int token_id = static_cast<int>(i);
					if (token_id < m_vocab.token_sot + 1 || token_id >= m_vocab.token_sot + 100)
					{
						logits[i] = -FLT_MAX;
					}
				}
				break;

			case DecoderState::SeekingTimestamp:
				// When seeking timestamp, only allow timestamp tokens (ID >= token_beg)
				for (size_t i = 0; i < static_cast<size_t>(m_vocab.token_beg); ++i)
				{
					logits[i] = -FLT_MAX;
				}
				break;

			case DecoderState::Transcribing:
				// When transcribing text, suppress special tokens that shouldn't appear
				// BUT allow timestamp tokens to naturally appear when needed!

				// Suppress SOT token (shouldn't appear during transcription)
				if (m_vocab.token_sot >= 0 && static_cast<size_t>(m_vocab.token_sot) < logits_size)
				{
					logits[m_vocab.token_sot] = -FLT_MAX;
				}

				// Suppress EOT token during normal transcription (prevents EOT loops!)
				if (m_vocab.token_eot >= 0 && static_cast<size_t>(m_vocab.token_eot) < logits_size)
				{
					logits[m_vocab.token_eot] = -FLT_MAX;
				}

				// Suppress no-timestamps token
				if (m_vocab.token_not >= 0 && static_cast<size_t>(m_vocab.token_not) < logits_size)
				{
					logits[m_vocab.token_not] = -FLT_MAX;
				}

				// Suppress language tokens during transcription
				for (int lang_token = m_vocab.token_sot + 1; lang_token < m_vocab.token_sot + 100; ++lang_token)
				{
					if (lang_token >= 0 && static_cast<size_t>(lang_token) < logits_size)
					{
						logits[lang_token] = -FLT_MAX;
					}
				}

				// NOTE: We do NOT suppress timestamp tokens here!
				// Timestamp tokens should be allowed during transcription to naturally end segments
				break;
		}
	}

	void WhisperSampler::suppress_out_of_range_timestamps(float* logits, size_t logits_size, int current_seek, int seek_end)
	{
		// CRITICAL FIX: Proper timestamp range constraints based on whisper.cpp logic
		// Timestamps must be monotonically increasing and within the valid segment range
		// Based on the formula: seek_delta = 2 * (token_id - token_beg)

		const int token_beg = m_vocab.token_beg;

		// CRITICAL: Minimum timestamp token should correspond to current_seek position
		// This prevents going back in time and ensures monotonic progression
		const int min_valid_token = token_beg + current_seek / 2;
		const int max_valid_token = token_beg + seek_end / 2;

		int suppressed_count = 0;

		// Suppress timestamp tokens outside the valid range
		for (int token_id = token_beg; token_id >= 0 && token_id < static_cast<int>(logits_size); ++token_id)
		{
			if (token_id < min_valid_token || token_id > max_valid_token)
			{
				logits[token_id] = -FLT_MAX;
				suppressed_count++;
			}
		}

		// Timestamp range constraints applied
	}

	void WhisperSampler::suppress_initial_timestamp(float* logits, size_t logits_size, float max_initial_ts)
	{
		// CRITICAL: Implement max_initial_ts constraint for initial timestamp sampling
		// This prevents the first timestamp from jumping too far into the audio
		// Based on whisper.cpp logic: https://github.com/openai/whisper/blob/main/whisper/decoding.py#L426-L429

		if (max_initial_ts <= 0.0f)
			return; // No constraint

		const int token_beg = m_vocab.token_beg;

		// Calculate precision and maximum allowed initial timestamp token
		// Using WHISPER_CHUNK_SIZE = 30 and n_audio_ctx = 1500 (for small model)
		const float precision = 30.0f / 1500.0f; // WHISPER_CHUNK_SIZE / n_audio_ctx
		const int tid0 = static_cast<int>(std::round(max_initial_ts / precision));

		// Suppress all timestamp tokens beyond max_initial_ts
		for (int i = token_beg + tid0 + 1; i < static_cast<int>(logits_size); ++i)
		{
			logits[i] = -FLT_MAX;
		}
	}

	bool WhisperSampler::should_force_timestamp_sampling(const float* logits, size_t logits_size) const
	{
		// CRITICAL: Dynamic timestamp sampling logic from Const-me/Whisper (original project)
		// This matches the exact logic in ContextImpl::sampleBest function
		// Uses probability values (not log probabilities) like the original

		const int token_beg = m_vocab.token_beg;
		const int n_logits = static_cast<int>(logits_size);

		// Step 1: Convert logits to probabilities (softmax)
		std::vector<float> probs(n_logits);

		// Find max logit for numerical stability
		float logit_max = -FLT_MAX;
		for (int i = 0; i < n_logits; ++i) {
			if (logits[i] > -FLT_MAX && logits[i] > logit_max) {
				logit_max = logits[i];
			}
		}

		// Compute softmax
		float sum_exp = 0.0f;
		for (int i = 0; i < n_logits; ++i) {
			if (logits[i] > -FLT_MAX) {
				probs[i] = expf(logits[i] - logit_max);
				sum_exp += probs[i];
			} else {
				probs[i] = 0.0f;
			}
		}

		// Normalize to get probabilities
		for (int i = 0; i < n_logits; ++i) {
			probs[i] /= sum_exp;
		}

		// Step 2: Compute timestamp probability sum (sum_ts)
		double sum_ts = 0.0;
		for (int i = token_beg; i < n_logits; ++i) {
			sum_ts += probs[i];
		}

		// Step 3: Find max text token probability (max_tx)
		double max_tx = -1.0;
		for (int i = 0; i < token_beg; ++i) {
			max_tx = std::max(max_tx, (double)probs[i]);
		}

		// Step 4: Decision logic from original project
		// "if the probability sum of all timestamp tokens is higher than the max probability of the text tokens"
		bool force_timestamp = (sum_ts > max_tx);

		// DEBUG: Log the decision process
		static int debug_count = 0;
		if (debug_count < 5) {
			printf("DYNAMIC_SAMPLING_DEBUG[%d]: sum_ts=%.6f, max_tx=%.6f, force_timestamp=%s\n",
				debug_count, sum_ts, max_tx, force_timestamp ? "YES" : "NO");
			debug_count++;
		}

		return force_timestamp;
	}

	bool WhisperSampler::should_force_timestamp_sampling_conservative(const float* logits, size_t logits_size) const
	{
		// CRITICAL: Conservative dynamic timestamp sampling
		// Only force timestamp if the difference is very significant
		// This prevents over-aggressive timestamp forcing

		const int token_beg = m_vocab.token_beg;
		const int n_logits = static_cast<int>(logits_size);

		// Step 1: Convert logits to probabilities (softmax)
		std::vector<float> probs(n_logits);

		// Find max logit for numerical stability
		float logit_max = -FLT_MAX;
		for (int i = 0; i < n_logits; ++i) {
			if (logits[i] > -FLT_MAX && logits[i] > logit_max) {
				logit_max = logits[i];
			}
		}

		// Compute softmax
		float sum_exp = 0.0f;
		for (int i = 0; i < n_logits; ++i) {
			if (logits[i] > -FLT_MAX) {
				probs[i] = expf(logits[i] - logit_max);
				sum_exp += probs[i];
			} else {
				probs[i] = 0.0f;
			}
		}

		// Normalize to get probabilities
		for (int i = 0; i < n_logits; ++i) {
			probs[i] /= sum_exp;
		}

		// Step 2: Compute timestamp probability sum (sum_ts)
		double sum_ts = 0.0;
		for (int i = token_beg; i < n_logits; ++i) {
			sum_ts += probs[i];
		}

		// Step 3: Find max text token probability (max_tx)
		double max_tx = -1.0;
		for (int i = 0; i < token_beg; ++i) {
			max_tx = std::max(max_tx, (double)probs[i]);
		}

		// Step 4: Ultra-conservative decision logic
		// Only force timestamp if BOTH conditions are met:
		// 1. Timestamp probability is very high (> 10%)
		// 2. The difference is extremely significant (factor of 100)
		// This should rarely trigger, allowing normal text generation
		bool force_timestamp = (sum_ts > 0.10) && (sum_ts > max_tx * 100.0);

		// Dynamic timestamp sampling decision made (currently disabled)

		return force_timestamp;
	}

	bool WhisperSampler::should_force_timestamp_sampling_original(const float* logits, size_t logits_size) const
	{
		// CRITICAL: Original project's simple dynamic timestamp sampling logic
		// This is the EXACT logic from Const-me/Whisper ContextImpl::sampleBest
		// Simple rule: if sum_ts > max_tx, force timestamp sampling

		const int token_beg = m_vocab.token_beg;
		const int n_logits = static_cast<int>(logits_size);

		// Step 1: Convert logits to probabilities (softmax)
		std::vector<float> probs(n_logits);

		// Find max logit for numerical stability
		float logit_max = -FLT_MAX;
		for (int i = 0; i < n_logits; ++i) {
			if (logits[i] > -FLT_MAX && logits[i] > logit_max) {
				logit_max = logits[i];
			}
		}

		// Compute softmax probabilities
		float sum_exp = 0.0f;
		for (int i = 0; i < n_logits; ++i) {
			if (logits[i] > -FLT_MAX) {
				probs[i] = expf(logits[i] - logit_max);
				sum_exp += probs[i];
			}
			else {
				probs[i] = 0.0f;
			}
		}

		// Normalize probabilities
		if (sum_exp > 0.0f) {
			for (int i = 0; i < n_logits; ++i) {
				probs[i] /= sum_exp;
			}
		}

		// Step 2: Calculate timestamp probability sum
		double sum_ts = 0.0;
		for (int i = token_beg; i < n_logits; ++i) {
			sum_ts += probs[i];
		}

		// Step 3: Find maximum text token probability
		double max_tx = -1.0;
		for (int i = 0; i < token_beg; ++i) {
			if (probs[i] > max_tx) {
				max_tx = probs[i];
			}
		}

		// Step 4: Original project's simple decision logic
		// EXACT logic from ContextImpl::sampleBest: if( sum_ts > max_tx || force_timestamp )
		bool force_timestamp = (sum_ts > max_tx);

		return force_timestamp;
	}

	void WhisperSampler::force_timestamp_sampling(float* logits, size_t logits_size)
	{
		// Force timestamp sampling by suppressing all text tokens
		// This implements the whisper.cpp logic when timestamp_logprob > max_text_token_logprob
		const int token_beg = m_vocab.token_beg;

		for (int i = 0; i < token_beg; ++i)
		{
			logits[i] = -FLT_MAX;
		}
	}

	void WhisperSampler::apply_language_constraints(float* logits, size_t logits_size, int language_token)
	{
		// Apply language-specific constraints based on the selected language
		// This is the CRITICAL FIX for Chinese transcription issues

		// Calculate language ID from token (language_token = token_sot + 1 + lang_id)
		const int lang_id = language_token - m_vocab.token_sot - 1;

		// Chinese language ID is 1 (from whisper.cpp g_lang map)
		if (lang_id == 1) // Chinese
		{
			// Applying Chinese language constraints

			// Strategy: Boost Chinese character tokens and suppress non-Chinese content
			int chinese_tokens_boosted = 0;
			int music_tokens_suppressed = 0;

			for (size_t i = 0; i < logits_size; ++i)
			{
				int token_id = static_cast<int>(i);

				// Safety check: ensure token_id is valid
				if (token_id < 0)
				{
					continue;
				}

				// Skip special tokens (they have their own suppression logic)
				if (m_vocab.isSpecial(token_id))
				{
					continue;
				}

				// Boost Chinese character tokens
				if (is_chinese_token(token_id))
				{
					logits[i] += 2.0f; // Significant boost for Chinese characters
					chinese_tokens_boosted++;
				}
				// Suppress music/sound effect tokens
				else if (is_music_token(token_id))
				{
					logits[i] -= 5.0f; // Strong suppression for music tokens
					music_tokens_suppressed++;
				}
				// Mild suppression for English-like tokens (but don't completely block them)
				else
				{
					const char* token_text = m_vocab.string(token_id);
					if (token_text && strlen(token_text) > 0)
					{
						// Check if token contains only ASCII characters (likely English)
						bool is_ascii_only = true;
						for (const char* p = token_text; *p; ++p)
						{
							if (static_cast<unsigned char>(*p) > 127)
							{
								is_ascii_only = false;
								break;
							}
						}

						if (is_ascii_only && strlen(token_text) > 1)
						{
							logits[i] -= 1.0f; // Mild suppression for English words
						}
					}
				}
			}

			// Language constraints applied
		}
		// Add support for other languages here in the future
	}

	bool WhisperSampler::is_chinese_token(int token_id) const
	{
		const char* token_text = m_vocab.string(token_id);
		if (!token_text || strlen(token_text) == 0)
		{
			return false;
		}

		// Check if token contains Chinese characters (Unicode range)
		// Chinese characters are typically in ranges:
		// - CJK Unified Ideographs: U+4E00-U+9FFF
		// - CJK Extension A: U+3400-U+4DBF
		// In UTF-8, these will have specific byte patterns

		const unsigned char* text = reinterpret_cast<const unsigned char*>(token_text);
		for (size_t i = 0; text[i]; )
		{
			// Check for UTF-8 encoded Chinese characters
			if (text[i] >= 0xE4 && text[i] <= 0xE9) // Common Chinese character range in UTF-8
			{
				if (text[i+1] >= 0x80 && text[i+1] <= 0xBF &&
					text[i+2] >= 0x80 && text[i+2] <= 0xBF)
				{
					// This is likely a Chinese character
					return true;
				}
			}

			// Move to next character (handle UTF-8 multi-byte sequences)
			if (text[i] < 0x80) i += 1;      // ASCII
			else if (text[i] < 0xE0) i += 2; // 2-byte UTF-8
			else if (text[i] < 0xF0) i += 3; // 3-byte UTF-8
			else i += 4;                     // 4-byte UTF-8
		}

		return false;
	}

	bool WhisperSampler::is_music_token(int token_id) const
	{
		const char* token_text = m_vocab.string(token_id);
		if (!token_text)
		{
			return false;
		}

		// Check for common music/sound effect tokens
		std::string text(token_text);

		// Convert to lowercase for case-insensitive comparison
		std::transform(text.begin(), text.end(), text.begin(), ::tolower);

		// Common music/sound effect indicators
		return text.find("music") != std::string::npos ||
			   text.find("[music]") != std::string::npos ||
			   text.find("sound") != std::string::npos ||
			   text.find("noise") != std::string::npos ||
			   text.find("applause") != std::string::npos ||
			   text.find("laughter") != std::string::npos ||
			   text.find("singing") != std::string::npos ||
			   text.find("instrumental") != std::string::npos;
	}
}
