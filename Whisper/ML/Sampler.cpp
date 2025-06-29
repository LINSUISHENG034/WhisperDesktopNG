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

	int WhisperSampler::sample(float* logits, size_t logits_size, const std::vector<int>& history_tokens)
	{
		if (!logits || logits_size == 0)
		{
			return 0; // Return safe default
		}

		// 1. Apply adaptive repetition penalty
		apply_repetition_penalty(logits, logits_size, history_tokens);

		// 2. Apply temperature scaling
		apply_temperature(logits, logits_size);

		// 3. Find the best token directly (greedy sampling)
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
}
