#include "textWriter.h"
#include "../../ComLightLib/comLightClient.h"
#include <array>
#define WIN32_LEAN_AND_MEAN
#include <pathcch.h>
#include <atlstr.h>
#include <atlfile.h>
#pragma comment(lib, "Pathcch.lib")

namespace
{
	HRESULT replaceExtension( CString& path, LPCTSTR inputPath, LPCTSTR ext )
	{
		path = inputPath;

		const size_t len = (size_t)path.GetLength() + 4;
		wchar_t* buffer = path.GetBufferSetLength( (int)len );
		const HRESULT hr = PathCchRenameExtension( buffer, len, ext );
		path.ReleaseBuffer();
		return hr;
	}

	HRESULT generateCustomPath( CString& path, LPCTSTR inputPath, LPCTSTR ext, LPCTSTR pattern, LPCTSTR modelPath, LPCTSTR gpu )
	{
		if( !pattern || wcslen( pattern ) == 0 )
		{
			// Fall back to default behavior
			return replaceExtension( path, inputPath, ext );
		}

		// Extract input filename without extension
		CString inputName = inputPath;
		int lastSlash = inputName.ReverseFind( L'\\' );
		if( lastSlash == -1 ) lastSlash = inputName.ReverseFind( L'/' );
		if( lastSlash != -1 ) inputName = inputName.Mid( lastSlash + 1 );

		int lastDot = inputName.ReverseFind( L'.' );
		if( lastDot != -1 ) inputName = inputName.Left( lastDot );

		// Extract model name from path
		CString modelName = modelPath;
		int modelSlash = modelName.ReverseFind( L'\\' );
		if( modelSlash == -1 ) modelSlash = modelName.ReverseFind( L'/' );
		if( modelSlash != -1 ) modelName = modelName.Mid( modelSlash + 1 );

		int modelDot = modelName.ReverseFind( L'.' );
		if( modelDot != -1 ) modelName = modelName.Left( modelDot );

		// Generate timestamp
		SYSTEMTIME st;
		GetLocalTime( &st );
		CString timestamp;
		timestamp.Format( L"%04d%02d%02d_%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond );

		// Determine backend type
		CString backend = (gpu && wcslen( gpu ) > 0) ? L"gpu" : L"cpu";

		// Replace placeholders in pattern
		path = pattern;
		path.Replace( L"{input}", inputName );
		path.Replace( L"{model}", modelName );
		path.Replace( L"{gpu}", backend );
		path.Replace( L"{time}", timestamp );

		// Add extension if not present
		if( path.Find( L'.' ) == -1 )
		{
			path += ext;
		}

		return S_OK;
	}

	// Abstract base class for text writers
	class Writer
	{
	protected:
		CAtlFile file;
		virtual HRESULT impl( const Whisper::sSegment* const segments, const size_t length ) = 0;

	public:
		HRESULT write( Whisper::iContext* context, LPCTSTR audioPath, LPCTSTR ext, LPCTSTR pattern = nullptr, LPCTSTR modelPath = nullptr, LPCTSTR gpu = nullptr )
		{
			CString path;
			CHECK( generateCustomPath( path, audioPath, ext, pattern, modelPath, gpu ) );
			CHECK( file.Create( path, GENERIC_WRITE, 0, CREATE_ALWAYS ) );

			using namespace Whisper;

			const eResultFlags resultFlags = eResultFlags::Timestamps | eResultFlags::Tokens;
			ComLight::CComPtr<iTranscribeResult> result;
			CHECK( context->getResults( resultFlags, &result ) );

			sTranscribeLength len;
			CHECK( result->getSize( len ) );
			const sSegment* const segments = result->getSegments();

			return impl( segments, len.countSegments );
		}
	};

	HRESULT writeUtf8Bom( CAtlFile& file )
	{
		const std::array<uint8_t, 3> bom = { 0xEF, 0xBB, 0xBF };
		return file.Write( bom.data(), 3 );
	}

	void printTime( CStringA& rdi, Whisper::sTimeSpan time, bool comma = false )
	{
		Whisper::sTimeSpanFields fields = time;
		const uint32_t hours = fields.days * 24 + fields.hours;
		const char separator = comma ? ',' : '.';
		rdi.AppendFormat( "%02d:%02d:%02d%c%03d",
			(int)hours,
			(int)fields.minutes,
			(int)fields.seconds,
			separator,
			fields.ticks / 10'000 );
	}

	const char* skipBlank( const char* rsi )
	{
		while( true )
		{
			const char c = *rsi;
			if( c == ' ' || c == '\t' )
			{
				rsi++;
				continue;
			}
			return rsi;
		}
	}

	inline const char* cstr( const CStringA& s ) { return s; }

	HRESULT writeString( CAtlFile& file, const CStringA& line )
	{
		if( line.GetLength() > 0 )
			CHECK( file.Write( cstr( line ), (DWORD)line.GetLength() ) );
		return S_OK;
	}

	// Writer for UTF-8 text files
	class TextWriter : public Writer
	{
		const bool timestamps;

		HRESULT impl( const Whisper::sSegment* const segments, const size_t length ) override final
		{
			CHECK( writeUtf8Bom( file ) );
			using namespace Whisper;

			CStringA line;
			for( size_t i = 0; i < length; i++ )
			{
				const sSegment& seg = segments[ i ];

				if( timestamps )
				{
					line = "[";
					printTime( line, seg.time.begin );
					line += " --> ";
					printTime( line, seg.time.end );
					line += "]  ";
				}
				else
					line = "";

				line += skipBlank( seg.text );
				line += "\r\n";
				CHECK( writeString( file, line ) );
			}
			return S_OK;
		}
	public:
		TextWriter( bool tt ) : timestamps( tt ) { }
	};

	// Writer for SubRip format: https://en.wikipedia.org/wiki/SubRip#SubRip_file_format
	class SubRipWriter : public Writer
	{
		HRESULT impl( const Whisper::sSegment* const segments, const size_t length ) override final
		{
			CHECK( writeUtf8Bom( file ) );
			using namespace Whisper;

			CStringA line;
			for( size_t i = 0; i < length; i++ )
			{
				const sSegment& seg = segments[ i ];

				line.Format( "%zu\r\n", i + 1 );
				printTime( line, seg.time.begin, true );
				line += " --> ";
				printTime( line, seg.time.end, true );
				line += "\r\n";
				line += skipBlank( seg.text );
				line += "\r\n\r\n";
				CHECK( writeString( file, line ) );
			}
			return S_OK;
		}
	};

	// Writer for WebVTT format: https://en.wikipedia.org/wiki/WebVTT
	class VttWriter : public Writer
	{
		HRESULT impl( const Whisper::sSegment* const segments, const size_t length ) override final
		{
			CHECK( writeUtf8Bom( file ) );
			using namespace Whisper;

			CStringA line;
			line = "WEBVTT\r\n\r\n";
			CHECK( writeString( file, line ) );

			for( size_t i = 0; i < length; i++ )
			{
				const sSegment& seg = segments[ i ];
				line = "";

				printTime( line, seg.time.begin );
				line += " --> ";
				printTime( line, seg.time.end );
				line += "\r\n";
				line += skipBlank( seg.text );
				line += "\r\n\r\n";
				CHECK( writeString( file, line ) );
			}
			return S_OK;
		}
	};
}

HRESULT writeText( Whisper::iContext* context, LPCTSTR audioPath, bool timestamps, LPCTSTR pattern, LPCTSTR modelPath, LPCTSTR gpu )
{
	TextWriter writer{ timestamps };
	return writer.write( context, audioPath, L".txt", pattern, modelPath, gpu );
}

HRESULT writeSubRip( Whisper::iContext* context, LPCTSTR audioPath, LPCTSTR pattern, LPCTSTR modelPath, LPCTSTR gpu )
{
	SubRipWriter writer;
	return writer.write( context, audioPath, L".srt", pattern, modelPath, gpu );
}

HRESULT writeWebVTT( Whisper::iContext* context, LPCTSTR audioPath, LPCTSTR pattern, LPCTSTR modelPath, LPCTSTR gpu )
{
	VttWriter writer;
	return writer.write( context, audioPath, L".vtt", pattern, modelPath, gpu );
}