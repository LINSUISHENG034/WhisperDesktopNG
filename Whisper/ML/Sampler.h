#pragma once
#include <vector>
#include <memory>
#include "../API/sParams.h"

namespace Whisper
{
	class Vocabulary;

	// Forward declaration of DecoderState from ContextImpl.h
	enum class DecoderState {
		SeekingSOT,        // Looking for Start of Transcript token
		SeekingLanguage,   // Looking for language token after SOT
		Transcribing,      // Generating normal text content
		SeekingTimestamp   // Looking for timestamp token
	};

	/// <summary>Advanced token sampler for Whisper models with support for various sampling strategies</summary>
	class WhisperSampler
	{
	public:
		/// <summary>Constructs a new WhisperSampler with the specified parameters</summary>
		/// <param name="params">Sampling parameters configuration</param>
		/// <param name="vocab">Reference to vocabulary for token classification</param>
		WhisperSampler(const SamplingParams& params, const Vocabulary& vocab);

		/// <summary>Destructor</summary>
		~WhisperSampler();

		/// <summary>Samples the next token from logits using configured sampling strategy</summary>
		/// <param name="logits">Array of logits for all tokens (will be modified in-place)</param>
		/// <param name="logits_size">Size of the logits array</param>
		/// <param name="history_tokens">Recent token history for repetition penalty</param>
		/// <param name="state">Current decoder state for token suppression</param>
		/// <returns>Selected token ID</returns>
		int sample(float* logits, size_t logits_size, const std::vector<int>& history_tokens, DecoderState state);

		/// <summary>Backward compatibility: Samples token with default Transcribing state</summary>
		/// <param name="logits">Array of logits for all tokens (will be modified in-place)</param>
		/// <param name="logits_size">Size of the logits array</param>
		/// <param name="history_tokens">Recent token history for repetition penalty</param>
		/// <returns>Selected token ID</returns>
		int sample(float* logits, size_t logits_size, const std::vector<int>& history_tokens);



		/// <summary>Updates sampling parameters</summary>
		/// <param name="params">New sampling parameters</param>
		void updateParams(const SamplingParams& params);

		/// <summary>Gets current sampling parameters</summary>
		/// <returns>Current sampling parameters</returns>
		const SamplingParams& getParams() const;

	private:
		/// <summary>Current sampling parameters</summary>
		SamplingParams m_params;
		
		/// <summary>Reference to vocabulary for token classification</summary>
		const Vocabulary& m_vocab;

		/// <summary>Suppresses inappropriate tokens based on current decoder state</summary>
		/// <param name="logits">Array of logits to modify</param>
		/// <param name="logits_size">Size of the logits array</param>
		/// <param name="state">Current decoder state</param>
		void suppress_tokens(float* logits, size_t logits_size, DecoderState state);

		/// <summary>Applies repetition penalty to logits based on token history</summary>
		/// <param name="logits">Array of logits to modify</param>
		/// <param name="logits_size">Size of the logits array</param>
		/// <param name="history_tokens">Recent token history</param>
		void apply_repetition_penalty(float* logits, size_t logits_size, const std::vector<int>& history_tokens);

		/// <summary>Applies temperature scaling to logits</summary>
		/// <param name="logits">Array of logits to modify</param>
		/// <param name="logits_size">Size of the logits array</param>
		void apply_temperature(float* logits, size_t logits_size);

		/// <summary>Performs greedy sampling to find the best token</summary>
		/// <param name="logits">Array of logits</param>
		/// <param name="logits_size">Size of the logits array</param>
		/// <returns>Token ID with highest probability</returns>
		int greedy_sample(const float* logits, size_t logits_size);

		/// <summary>Validates token ID is within valid range</summary>
		/// <param name="token_id">Token ID to validate</param>
		/// <param name="logits_size">Maximum valid token ID</param>
		/// <returns>True if token ID is valid</returns>
		bool is_valid_token(int token_id, size_t logits_size) const;
	};
}
