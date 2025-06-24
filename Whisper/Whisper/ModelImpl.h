#pragma once
#include "../API/iContext.cl.h"
#include "../ComLightLib/comLightServer.h"
#include "WhisperModel.h"
#include "../ComLightLib/streams.h"
#include "../ML/Device.h"

namespace Whisper
{
	using ComLight::iReadStream;

	class ModelImpl : public ComLight::ObjectRoot<iModel>
	{
		DirectCompute::Device device;
		WhisperModel model;
		const uint32_t gpuFlags;
		const std::wstring adapter;
		std::wstring modelPath;  // Store model path for WhisperCppEncoder creation

		HRESULT COMLIGHTCALL createContext( iContext** pp ) override final;

		HRESULT COMLIGHTCALL tokenize( const char* text, pfnDecodedTokens pfn, void* pv ) override final;

		HRESULT COMLIGHTCALL getSpecialTokens( SpecialTokens& rdi ) override final
		{
			model.shared->vocab.getSpecialTokens( rdi );
			return S_OK;
		}

		HRESULT COMLIGHTCALL isMultilingual() override final
		{
			return model.shared->vocab.is_multilingual() ? S_OK : S_FALSE;
		}

		const char* COMLIGHTCALL stringFromToken( whisper_token token ) override final
		{
			return model.shared->vocab.string( token );
		}

		static inline std::wstring makeString( const wchar_t* p )
		{
			if( p == nullptr )
				return std::wstring{};
			else
				return std::wstring{ p };
		}

		HRESULT createClone( const ModelImpl& source );
		HRESULT COMLIGHTCALL clone( iModel** rdi ) override final;

	public:
		void setModelPath( const wchar_t* path ) {
			modelPath = path ? path : L"";
			// B.1 LOG: setModelPath被调用
			printf("[DEBUG] ModelImpl::setModelPath: path=%ls\n", modelPath.c_str());
			fflush(stdout);
		}
		ModelImpl( const sModelSetup& setup ) :
			gpuFlags( setup.flags ),
			adapter( makeString( setup.adapter ) )
		{ }

		ModelImpl( const ModelImpl& source ) :
			gpuFlags( source.gpuFlags ),
			adapter( source.adapter )
		{ }

		void FinalRelease();

		HRESULT load( iReadStream* stm, bool hybrid, const sLoadModelCallbacks* callbacks );
	};
}