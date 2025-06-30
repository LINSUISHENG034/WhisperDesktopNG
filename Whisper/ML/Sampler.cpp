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
		: m_params(params), m_vocab(vocab)
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

	int WhisperSampler::sample(float* logits, size_t logits_size, const std::vector<int>& history_tokens, DecoderState state, int current_seek, int seek_end)
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

		// 3. Apply adaptive repetition penalty
		apply_repetition_penalty(logits, logits_size, history_tokens);

		// 4. Apply temperature scaling
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
				if (static_cast<size_t>(m_vocab.token_sot) < logits_size)
				{
					logits[m_vocab.token_sot] = -FLT_MAX;
				}

				// Suppress EOT token during normal transcription (prevents EOT loops!)
				if (static_cast<size_t>(m_vocab.token_eot) < logits_size)
				{
					logits[m_vocab.token_eot] = -FLT_MAX;
				}

				// Suppress no-timestamps token
				if (static_cast<size_t>(m_vocab.token_not) < logits_size)
				{
					logits[m_vocab.token_not] = -FLT_MAX;
				}

				// Suppress language tokens during transcription
				for (int lang_token = m_vocab.token_sot + 1; lang_token < m_vocab.token_sot + 100; ++lang_token)
				{
					if (static_cast<size_t>(lang_token) < logits_size)
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
		// Suppress timestamp tokens that would result in positions outside the valid range
		// Based on the formula: seek_delta = 2 * (token_id - token_beg)
		// We want: current_seek <= seek_delta <= seek_end

		const int token_beg = m_vocab.token_beg;
		const int min_valid_token = token_beg;  // Timestamps start from 0
		const int max_valid_token = token_beg + (seek_end - current_seek) / 2;

		// DEBUG: Log timestamp range constraints
		printf("TIMESTAMP_RANGE: current_seek=%d, seek_end=%d, token_beg=%d, min_valid=%d, max_valid=%d\n",
			current_seek, seek_end, token_beg, min_valid_token, max_valid_token);

		// DEBUG: Log top timestamp token probabilities before suppression
		printf("TIMESTAMP_LOGITS_BEFORE: Top 10 timestamp tokens:\n");
		std::vector<std::pair<float, int>> timestamp_probs;
		for (int token_id = token_beg; token_id < static_cast<int>(logits_size) && token_id < token_beg + 100; ++token_id)
		{
			timestamp_probs.push_back({logits[token_id], token_id});
		}
		std::sort(timestamp_probs.begin(), timestamp_probs.end(), std::greater<std::pair<float, int>>());
		for (int i = 0; i < std::min(10, (int)timestamp_probs.size()); ++i)
		{
			float prob = timestamp_probs[i].first;
			int token_id = timestamp_probs[i].second;
			int time_cs = 2 * (token_id - token_beg);  // centiseconds
			printf("  Token %d (%.1fs): logit=%.6f\n", token_id, time_cs / 100.0f, prob);
		}

		// DEBUG: Also check the range of all logits values
		float min_logit = FLT_MAX, max_logit = -FLT_MAX;
		for (int token_id = token_beg; token_id < static_cast<int>(logits_size) && token_id < token_beg + 100; ++token_id)
		{
			min_logit = std::min(min_logit, logits[token_id]);
			max_logit = std::max(max_logit, logits[token_id]);
		}
		printf("TIMESTAMP_LOGITS_RANGE: min=%.6f, max=%.6f\n", min_logit, max_logit);

		int suppressed_count = 0;
		// Suppress timestamp tokens outside the valid range
		for (int token_id = token_beg; token_id < static_cast<int>(logits_size); ++token_id)
		{
			if (token_id < min_valid_token || token_id > max_valid_token)
			{
				logits[token_id] = -FLT_MAX;
				suppressed_count++;
			}
		}

		printf("TIMESTAMP_RANGE: suppressed %d timestamp tokens\n", suppressed_count);
	}
}
