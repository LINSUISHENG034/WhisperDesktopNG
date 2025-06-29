#pragma once

namespace Whisper
{
	/// <summary>Sampling parameters for token generation</summary>
	struct SamplingParams
	{
		/// <summary>Temperature for sampling diversity. Higher values increase randomness.</summary>
		float temperature = 0.8f;

		/// <summary>Repetition penalty factor. Values > 1.0 discourage repetition.</summary>
		/// <remarks>This value now serves as base penalty or is replaced by adaptive logic.</remarks>
		float repetition_penalty = 1.1f;

		/// <summary>Number of recent tokens to consider for repetition penalty</summary>
		int history_size = 10;

		/// <summary>Top-K sampling parameter. Only consider the K most probable tokens.</summary>
		/// <remarks>Set to 0 to disable Top-K sampling. Default value 5 is proven effective.</remarks>
		int top_k = 5;

		// Future extensions can include:
		// float top_p = 0.9f;
		// float frequency_penalty = 0.0f;
		// float presence_penalty = 0.0f;

		/// <summary>Provides high-quality default parameters for desktop applications</summary>
		/// <returns>Default sampling parameters optimized for quality and performance</returns>
		static SamplingParams defaultParams()
		{
			return SamplingParams{};
		}

		/// <summary>Provides conservative parameters for production use</summary>
		/// <returns>Conservative sampling parameters with minimal randomness</returns>
		static SamplingParams conservativeParams()
		{
			SamplingParams params;
			params.temperature = 0.6f;
			params.repetition_penalty = 1.05f;
			params.history_size = 8;
			return params;
		}

		/// <summary>Provides aggressive parameters for creative applications</summary>
		/// <returns>Aggressive sampling parameters with higher diversity</returns>
		static SamplingParams creativeParams()
		{
			SamplingParams params;
			params.temperature = 1.0f;
			params.repetition_penalty = 1.2f;
			params.history_size = 12;
			return params;
		}
	};
}
