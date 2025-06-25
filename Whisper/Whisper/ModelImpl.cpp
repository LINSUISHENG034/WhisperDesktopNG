#include "stdafx.h"
#include "ModelImpl.h"
#include "ContextImpl.h"
#include <intrin.h>
#include "../Utils/ReadStream.h"
#include "../modelFactory.h"
#include "../iWhisperEncoder.h"
#include "../WhisperCppEncoder.h"
#include "../DirectComputeEncoder.h"
using namespace Whisper;

void ModelImpl::FinalRelease()
{
	device.destroy();
}

// H.4: Factory function to create appropriate encoder implementation
std::unique_ptr<iWhisperEncoder> ModelImpl::createEncoder()
{
	// Strategy: Try WhisperCpp first, fall back to DirectCompute if needed
	if( !modelPath.empty() )
	{
		try
		{
			// Convert wide string to UTF-8 for WhisperCppEncoder
			std::string utf8Path;
			int len = WideCharToMultiByte( CP_UTF8, 0, modelPath.c_str(), -1, nullptr, 0, nullptr, nullptr );
			if( len > 0 )
			{
				utf8Path.resize( len - 1 );
				WideCharToMultiByte( CP_UTF8, 0, modelPath.c_str(), -1, &utf8Path[0], len, nullptr, nullptr );
			}

			printf("[DEBUG] ModelImpl::createEncoder: Attempting WhisperCppEncoder with path: %s\n", utf8Path.c_str());
			auto encoder = std::make_unique<WhisperCppEncoder>( utf8Path );

			// Test if encoder is ready
			if( encoder->isReady() )
			{
				printf("[DEBUG] ModelImpl::createEncoder: WhisperCppEncoder created successfully\n");
				return std::move(encoder);
			}
			else
			{
				printf("[DEBUG] ModelImpl::createEncoder: WhisperCppEncoder not ready, falling back to DirectCompute\n");
			}
		}
		catch( const std::exception& e )
		{
			printf("[DEBUG] ModelImpl::createEncoder: WhisperCppEncoder creation failed with std::exception: %s\n", e.what());
		}
		catch( ... )
		{
			printf("[DEBUG] ModelImpl::createEncoder: WhisperCppEncoder creation failed with unknown exception\n");
		}
	}

	// Fallback to DirectCompute implementation
	printf("[DEBUG] ModelImpl::createEncoder: Creating DirectComputeEncoder\n");
	try
	{
		auto encoder = std::make_unique<DirectComputeEncoder>( device, model );
		printf("[DEBUG] ModelImpl::createEncoder: DirectComputeEncoder created successfully\n");
		return std::move(encoder);
	}
	catch( const std::exception& e )
	{
		printf("[ERROR] ModelImpl::createEncoder: DirectComputeEncoder creation failed: %s\n", e.what());
		return nullptr;
	}
	catch( ... )
	{
		printf("[ERROR] ModelImpl::createEncoder: DirectComputeEncoder creation failed with unknown exception\n");
		return nullptr;
	}
}

HRESULT COMLIGHTCALL ModelImpl::createContext( iContext** pp )
{
	// H.5: Unified factory-based approach using encoder interface
	printf("=== [DEBUG] ModelImpl::createContext ENTRY ===\n");
	fflush(stdout);

	auto ts = device.setForCurrentThread();
	ComLight::CComPtr<ComLight::Object<ContextImpl>> obj;
	iModel* m = this;

	// H.5: Create encoder using factory method with detailed logging
	printf("=== [DEBUG] ModelImpl::createContext: Calling createEncoder() factory method ===\n");
	fflush(stdout);
	auto encoder = createEncoder();
	if( encoder )
	{
		printf("[DEBUG] ModelImpl::createContext: Factory created encoder implementation: %s\n", encoder->getImplementationName());
		printf("[DEBUG] ModelImpl::createContext: Creating ContextImpl with encoder\n");
		CHECK( ComLight::Object<ContextImpl>::create( obj, device, model, m, std::move( encoder ) ) );
		printf("[DEBUG] ModelImpl::createContext: ContextImpl created successfully with encoder\n");
	}
	else
	{
		printf("[DEBUG] ModelImpl::createContext: Factory returned null, using original DirectCompute implementation\n");
		CHECK( ComLight::Object<ContextImpl>::create( obj, device, model, m ) );
		printf("[DEBUG] ModelImpl::createContext: ContextImpl created successfully without encoder\n");
	}

	obj.detach( pp );
	return S_OK;

#if 0 // H.4: Legacy code preserved for reference
	// Original implementation with fallback logic
	auto ts = device.setForCurrentThread();
	ComLight::CComPtr<ComLight::Object<ContextImpl>> obj;

	iModel* m = this;

	// Create WhisperCppEncoder instance if we have a model path
	if( !modelPath.empty() )
	{
		// B.1 LOG: ModelImpl::createContext尝试创建WhisperCppEncoder
		printf("[DEBUG] ModelImpl::createContext: Attempting to create WhisperCppEncoder with modelPath\n");
		fflush(stdout);

		try
		{
			// Convert wide string to UTF-8 for WhisperCppEncoder
			std::string utf8Path;
			int len = WideCharToMultiByte( CP_UTF8, 0, modelPath.c_str(), -1, nullptr, 0, nullptr, nullptr );
			if( len > 0 )
			{
				utf8Path.resize( len - 1 );
				WideCharToMultiByte( CP_UTF8, 0, modelPath.c_str(), -1, &utf8Path[0], len, nullptr, nullptr );
			}

			// B.1 LOG: 打印转换后的UTF-8路径
			printf("[DEBUG] ModelImpl::createContext: UTF-8 path: %s\n", utf8Path.c_str());

			auto encoder = std::make_unique<WhisperCppEncoder>( utf8Path );
			CHECK( ComLight::Object<ContextImpl>::create( obj, device, model, m, std::move( encoder ) ) );

			// B.1 LOG: WhisperCppEncoder创建成功
			printf("[DEBUG] ModelImpl::createContext: WhisperCppEncoder created successfully\n");
		}
		catch( const std::exception& e )
		{
			// If WhisperCppEncoder creation fails, fall back to original implementation
			printf("[DEBUG] ModelImpl::createContext: WhisperCppEncoder creation failed with std::exception: %s\n", e.what());
			fflush(stdout);
			CHECK( ComLight::Object<ContextImpl>::create( obj, device, model, m ) );
		}
		catch( ... )
		{
			// If WhisperCppEncoder creation fails, fall back to original implementation
			printf("[DEBUG] ModelImpl::createContext: WhisperCppEncoder creation failed with unknown exception, falling back to original implementation\n");
			fflush(stdout);
			CHECK( ComLight::Object<ContextImpl>::create( obj, device, model, m ) );
		}
	}
	else
	{
		// Fall back to original implementation if no model path
		printf("[DEBUG] ModelImpl::createContext: No modelPath available, using original implementation\n");
		CHECK( ComLight::Object<ContextImpl>::create( obj, device, model, m ) );
	}

	obj.detach( pp );
	return S_OK;
#endif
}


HRESULT COMLIGHTCALL ModelImpl::tokenize( const char* text, pfnDecodedTokens pfn, void* pv )
{
	std::vector<int> tokens;
	CHECK( model.shared->vocab.tokenize( text, tokens ) );

	if( !tokens.empty() )
		pfn( tokens.data(), (int)tokens.size(), pv );
	else
		pfn( nullptr, 0, pv );

	return S_OK;
}

HRESULT COMLIGHTCALL ModelImpl::clone( iModel** rdi )
{
	if( !device.gpuInfo.cloneableModel() )
	{
		logError( u8"iModel.clone requires the Cloneable model flag" );
		return HRESULT_FROM_WIN32( ERROR_NOT_SUPPORTED );
	}

	ComLight::CComPtr<ComLight::Object<ModelImpl>> obj;
	CHECK( ComLight::Object<ModelImpl>::create( obj, *this ) );
	CHECK( obj->createClone( *this ) );
	obj.detach( rdi );
	return S_OK;
}

HRESULT ModelImpl::createClone( const ModelImpl& source )
{
	auto ts = device.setForCurrentThread();
	CHECK( device.createClone( source.device ) );
	return model.createClone( source.model );
}

HRESULT ModelImpl::load( iReadStream* stm, bool hybrid, const sLoadModelCallbacks* callbacks )
{
	auto ts = device.setForCurrentThread();
	CHECK( device.create( gpuFlags, adapter ) );
	return model.load( stm, hybrid, callbacks );
}

inline bool hasSse41AndF16C()
{
	int cpu_info[ 4 ];
	__cpuid( cpu_info, 1 );

	// https://en.wikipedia.org/wiki/CPUID EAX=1: Processor Info and Feature Bits
	constexpr uint32_t sse41 = ( 1u << 19 );
	constexpr uint32_t f16c = ( 1u << 29 );

#ifdef __AVX__
	constexpr uint32_t requiredBits = sse41 | f16c;
#else
	constexpr uint32_t requiredBits = sse41;
#endif

	const uint32_t ecx = (uint32_t)cpu_info[ 2 ];
	return ( ecx & requiredBits ) == requiredBits;
}

// True when the current CPU is good enough to run the hybrid model
inline bool hasAvxAndFma()
{
	// AVX needs OS support to preserve the 32-bytes registers across context switches, CPU support alone ain't enough
	// Calling a kernel API to check that support
	// The magic number is from there: https://stackoverflow.com/a/35096938/126995
	if( 0 == ( GetEnabledXStateFeatures() & 4 ) )
		return false;

	// FMA3 and F16C
	int cpuInfo[ 4 ];
	__cpuid( cpuInfo, 1 );
	// The magic numbers are from "Feature Information" table on Wikipedia:
	// https://en.wikipedia.org/wiki/CPUID#EAX=1:_Processor_Info_and_Feature_Bits 
	constexpr int requiredBits = ( 1 << 12 ) | ( 1 << 29 );
	if( requiredBits != ( cpuInfo[ 2 ] & requiredBits ) )
		return false;

	// BMI1
	// https://en.wikipedia.org/wiki/CPUID#EAX=7,_ECX=0:_Extended_Features
	__cpuid( cpuInfo, 7 );
	if( 0 == ( cpuInfo[ 1 ] & ( 1 << 3 ) ) )
		return false;

	return true;
}

HRESULT __stdcall Whisper::loadGpuModel( const wchar_t* path, const sModelSetup& setup, const sLoadModelCallbacks* callbacks, iModel** pp )
{
	if( nullptr == path || nullptr == pp )
		return E_POINTER;

#if WHISPER_NG_USE_ONLY_CPP_IMPLEMENTATION
	// Simplified path: No CPU feature validation needed for WhisperCppEncoder
	printf("[DEBUG] loadGpuModel: Using simplified path - skipping CPU feature validation\n");
	const bool hybrid = false; // Force non-hybrid mode
#else
	// Original implementation with CPU feature validation
	const bool hybrid = setup.impl == eModelImplementation::Hybrid;
	if( hybrid )
	{
#if BUILD_HYBRID_VERSION
		if( !hasAvxAndFma() )
		{
			logError( u8"eModelImplementation.Hybrid model requires a CPU with AVX1, FMA3, F16C and BMI1 support" );
			return ERROR_HV_CPUID_FEATURE_VALIDATION;
		}
#else
		logError( u8"This build of the DLL doesn’t implement eModelImplementation.Hybrid model" );
		return E_NOTIMPL;
#endif
	}
	else if( !hasSse41AndF16C() )
	{
		logError( u8"eModelImplementation.GPU model requires a CPU with SSE 4.1 and F16C support" );
		return ERROR_HV_CPUID_FEATURE_VALIDATION;
	}
#endif

	ComLight::Object<ReadStream> stream;
	HRESULT hr = stream.open( path );
	if( FAILED( hr ) )
	{
		logError16( L"Unable to open model binary file \"%s\"", path );
		return hr;
	}

	ComLight::CComPtr<ComLight::Object<ModelImpl>> obj;
	CHECK( ComLight::Object<ModelImpl>::create( obj, setup ) );

	// B.1 LOG: loadGpuModel设置模型路径
	printf("[DEBUG] loadGpuModel: Setting model path: %ls\n", path);
	fflush(stdout);

	obj->setModelPath( path );  // Store the model path
	hr = obj->load( &stream, hybrid, callbacks );
	if( FAILED( hr ) )
	{
		logError16( L"Error loading the model from \"%s\"", path );
		return hr;
	}

	obj.detach( pp );
	logInfo16( L"Loaded model from \"%s\" to VRAM", path );
	return S_OK;
}