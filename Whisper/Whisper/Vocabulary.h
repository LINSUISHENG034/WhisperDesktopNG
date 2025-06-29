#pragma once
#include "../../ComLightLib/streams.h"
#include "../API/SpecialTokens.h"
#include "../Utils/MurmurHash3.h"

namespace Whisper
{
	class Vocabulary
	{
		std::vector<const char*> tokens;
		std::vector<char> stringData;
		using THashMap = CAtlMap<const char*, int, StringPtrTraits>;
		THashMap idFromToken;

		void addExtra( int index, const char* format, int i );

		void completeBuild();
	public:
		Vocabulary();

		int n_vocab = 51864;

		HRESULT load( ComLight::iReadStream* stm, int lengthInHeader );

		using id = int;

		id token_eot = 50256;
		id token_sot = 50257;
		id token_prev = 50360;
		id token_solm = 50361; // ??
		id token_not = 50362; // no timestamps
		id token_beg = 50363;

		// available tasks (made non-const for Large-v3 compatibility)
		id token_translate = 50358;
		id token_transcribe = 50359;

		bool is_multilingual() const
		{
			return n_vocab >= 51865;  // Support both 51865 and 51866 (Large-v3)
		}

		// Adjust special token IDs based on vocabulary size (for Large-v3 compatibility)
		void adjustTokenIds()
		{
			if (is_multilingual()) {
				// Calculate offset based on vocabulary expansion
				const int dt = (n_vocab == 51866) ? 2 : 1;  // Large-v3 has +2 tokens

				// Adjust all special tokens like whisper.cpp does
				token_eot += dt;
				token_sot += dt;
				token_translate += dt;
				token_transcribe += dt;
				token_solm += dt;
				token_prev += dt;
				token_not += dt;
				token_beg += dt;

				logDebug(u8"Adjusted special tokens for n_vocab=%d, dt=%d, token_beg=%d",
					n_vocab, dt, token_beg);
			}
		}

		const char* string( int id ) const
		{
			if( id >= 0 && id < (int)tokens.size() )
				return tokens[ id ];
			return nullptr;
		}

		int findId( const char* token ) const;
		int findId( const std::string& token ) const
		{
			return findId( token.c_str() );
		}

		size_t size() const
		{
			return tokens.size();
		}

		void getSpecialTokens( SpecialTokens& rdi ) const;

		// Check if a token is special (following whisper.cpp's whisper_is_special logic)
		bool isSpecial( int token_id ) const
		{
			// All special tokens have IDs >= token_sot (Start of Transcription)
			// This matches whisper.cpp's whisper_is_special() implementation
			return token_id >= token_sot;
		}

		// Get language token ID from language code (e.g., "zh" -> token ID)
		int languageTokenId( const char* lang_code ) const;

		size_t getMemoryUse() const
		{
			return vectorMemoryUse( tokens ) + vectorMemoryUse( stringData );
		}

		HRESULT tokenize( const std::string& text, std::vector<id>& tokens ) const;
	};
}