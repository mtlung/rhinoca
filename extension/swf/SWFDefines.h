#ifndef __SWFDefines__h__
#define __SWFDefines__h__

//------------------------------------------------------------------------------
typedef unsigned char UI8;
typedef unsigned short UI16;
typedef unsigned int UI32;
typedef signed int SI32;

//------------------------------------------------------------------------------
//	Remark: This header is NOT a complete SWF header
//	I do this since the RECT structure is variable length
// 	and if I add that I can NOT use a fread in 1 call
//------------------------------------------------------------------------------
typedef struct SWFHeader
{
	UI8		signature[3];
	UI8		version;
	UI32	fileLength;
} SWFHeader;

//------------------------------------------------------------------------------
const unsigned short MaxShortTagLength = 63;

//------------------------------------------------------------------------------
typedef struct RECT
{
	int xMin;
	int xMax;
	int yMin;
	int yMax;
} RECT;


//------------------------------------------------------------------------------
// Remark : Do NOT Modify unless u know what u are doing!
typedef enum TSwfTagType
{
		End					= 0
	,	ShowFrame			= 1
	,	DefineShape			= 2
	,	PlaceObject			= 4
	,	RemoveObject		= 5
	,	DefineBits			= 6
	,	DefineButton		= 7
	,	JPEGTables			= 8
	,	SetBackgroundColor	= 9
	,	DefineFont				= 10
	,	DefineText				= 11
	,	DoAction				= 12
	,	DefineFontInfo			= 13
	,	DefineSound				= 14
	,	StartSound				= 15
	,	DefineButtonSound		= 17
	,	SoundStreamHead			= 18
	,	SoundStreamBlock		= 19
	,	DefineBitsLossless	= 20
	,	DefineBitsJPEG2		= 21
	,	DefineShape2		= 22
	,	DefineButtonCxform	= 23
	,	Protect				= 24
	,	PlaceObject2		= 26
	,	RemoveObject2		= 28
	,	DefineShape3			= 32
	,	DefineText2				= 33
	,	DefineButton2			= 34
	,	DefineBitsJPEG3			= 35
	,	DefineBitsLossless2		= 36
	,	DefineEditText			= 37
	,	DefineSprite			= 39
	,	FrameLabel			= 43
	,	SoundStreamHead2	= 45
	,	DefineMorphShape	= 46
	,	DefineFont2			= 48
	,	ExportAssets			= 56
	,	ImportAssets			= 57
	,	EnableDebugger			= 58
	,	DoInitAction			= 59
	,	DefineVideoStream	= 60
	,	VideoFrame			= 61
	,	DefineFontInfo2		= 62
	,	EnableDebugger2		= 64
	,	ScriptLimits		= 65
	,	SetTabIndex			= 66
	,	FileAttributes		= 69
	,	PlaceObject3			= 70
	,	ImportAssets2			= 71
	,	DefineFontAlignZones	= 73
	,	CSMTextSettings			= 74
	,	DefineFont3				= 75
	,	SymbolClass				= 76
	,	Metadata				= 77
	,	DefineScalingGrid		= 78	
	,	DoABC							= 82
	,	DefineShape4					= 83
	,	DefineMorphShape2				= 84
	,	DefineSceneAndFrameLabelData	= 86
	,	DefineBinaryData				= 87
	,	DefineFontName					= 88
	,	StartSound2						= 89
	,	DefineBitsJPEG4		= 90
	,	DefineFont4			= 91
} TSwfTagType;


//------------------------------------------------------------------------------
#endif