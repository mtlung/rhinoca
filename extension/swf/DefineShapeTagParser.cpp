#include "pch.h"
#include "DefineShapeTagParser.h"
#include "SWFDefines.h"
#include "SWFParserUtils.h"

#include <cassert>

//------------------------------------------------------------------------------
TSwfTagType	DefineShapeTagParser::GetTagType() const
{
	return DefineShape;
}

//------------------------------------------------------------------------------
bool DefineShapeTagParser::Parse(const char * data, long length)
{
	int offset = 0;
	UI16 ShapeId = ReadData<UI16>( data, 0 );
	offset += sizeof(UI16);
	
	RECT ShapeBounds;
	int nBytes = ReadRECT( & ShapeBounds, data + offset);

	float xMinInPixels = (float)ShapeBounds.xMin / 20.0f;
	float xMaxInPixels = (float)ShapeBounds.xMax / 20.0f;
	float yMinInPixels = (float)ShapeBounds.yMin / 20.0f;
	float yMaxInPixels = (float)ShapeBounds.yMax / 20.0f;
	
	offset += nBytes;

	// shape with style
	UI8 FillStyleCount = 0;
	FillStyleCount = ReadData<UI8>( data, offset);

	offset += sizeof(UI8);

	if( 0xFF == FillStyleCount )
	{
		assert( !"Not supported" );	
	}

	for(int j=0;j<FillStyleCount;++j)
	{
		UI8 FillStyleType = ReadData<UI8>(data, offset); 		
		offset += sizeof(UI8);
		if( 0x00 == FillStyleType)
		{
			UI8 r = ReadData<UI8>(data,offset);
			offset += sizeof(UI8);
			UI8 g = ReadData<UI8>(data,offset);
			offset += sizeof(UI8);
			UI8 b = ReadData<UI8>(data,offset);
			offset += sizeof(UI8);			
		}	
		else if( 0x10==FillStyleType || 0x12==FillStyleType || 0x13==FillStyleType )
		{
			const char *ptr = data + offset;  
			ParserUtils::ReadMatrix( ptr ); // GradientMatrix			
		}
		break;
	}	

	return true;
}
//------------------------------------------------------------------------------