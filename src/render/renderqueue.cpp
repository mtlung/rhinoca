#include "pch.h"
#include "renderqueue.h"

namespace Render {

#pragma pack(push, 1)
struct sKey_Opaque
{
	rhuint8		LowZ;
	rhuint16	HighZ;
	rhuint8		MtlPass;
	rhuint16	Mtl;
	rhuint16	BaseKey;
};

struct sKey_Alpha
{
	rhuint8		MtlPass;
	rhuint16	Mtl;
	rhuint8		LowZ;
	rhuint16	HighZ;
	rhuint16	BaseKey;
};

struct sKey_Command
{
	rhuint32	Param;
	rhuint16	Priority;
	rhuint16	BaseKey;
};

union uKey
{
	rhuint64		Value;
	sKey_Opaque		Opaque;
	sKey_Alpha		Alpha;
	sKey_Command	Command;
};
#pragma pack(pop)

RenderItemKey::operator rhuint64()
{
	uKey A;
	rhuint16 BaseKey;

	BaseKey	=	(renderTarget	<< 11)	// 5 bits (32)
			|	(layer			<<  9)	// 2 bits ( 4)
			|	(viewport		<<  4)	// 5 bits (32)
			|	(viewportLayer	<<  2);	// 2 bits ( 4)
										// 1 bit = alpha
										// 1 bit = !cmd
	if (alphaBlend) 
		BaseKey |= 2;

	if (isCmd)
	{
		A.Command.BaseKey	= BaseKey;
		A.Command.Priority	= cmdPriority;
		A.Command.Param		= cmdParam;
	}
	else
	{
		BaseKey |= 1;

		if (alphaBlend) 
		{	// Translucent, sort back to front and by material
			int z = int( (1.0f-depth) * ((1<<24)-1) );

			A.Alpha.BaseKey		= BaseKey;
			A.Alpha.HighZ		= z >> 8;
			A.Alpha.LowZ		= z & 0xFF;
			A.Alpha.Mtl			= mtlId;
			A.Alpha.MtlPass		= mtlPass;
		}
		else
		{	// Opaque, sort by material and front to back
			int z = int( (depth) * ((1<<24)-1) );

			A.Opaque.BaseKey	= BaseKey;
			A.Opaque.HighZ		= z >> 8;
			A.Opaque.LowZ		= z & 0xFF;
			A.Opaque.Mtl		= mtlId;
			A.Opaque.MtlPass	= mtlPass;
		}
	}

	return A.Value;
}

void RenderItemKey::decode(rhuint64 Key)
{
	uKey	A;
	int		z;

	A.Value = Key;

	renderTarget	= (A.Opaque.BaseKey >> 11) & 0x001F;
	layer			= (A.Opaque.BaseKey >>  9) & 0x0003;
	viewport		= (A.Opaque.BaseKey >>  4) & 0x001F;
	viewportLayer	= (A.Opaque.BaseKey >>  2) & 0x0003;

	alphaBlend		= A.Opaque.BaseKey & 2;
	isCmd			= 0 == (A.Opaque.BaseKey & 1);

	if (isCmd)
	{
		cmdPriority = A.Command.Priority;
		cmdParam	= A.Command.Param;

		mtlId		= 0;
		mtlPass		= 0;
		depth		= 0.0f;
	}
	else
	{
		cmdPriority	= 0;
		cmdParam	= 0;

		if (alphaBlend)
		{	// Alpha blended
			z		= (A.Alpha.HighZ << 8) | (A.Alpha.LowZ);
			mtlId	=  A.Alpha.Mtl;
			mtlPass	=  A.Alpha.MtlPass;
			
			depth	= 1.0f - (z / float((1<<24)-1));
		}
		else
		{	// Opaque
			z		= (A.Opaque.HighZ << 8) | (A.Opaque.LowZ);
			mtlId	= A.Opaque.Mtl;
			mtlPass	= A.Opaque.MtlPass;

			depth	= z / float((1<<24)-1);
		}
	}
}

void RenderItemKey::makeOutOfRange()
{
	renderTarget	= -1;
	layer			= -1;
	viewport		= -1;
	viewportLayer	= -1;
	alphaBlend		= -1; 
	isCmd			= -1;
	
	cmdPriority		= -1;
	cmdParam		= -1;
	
	mtlId			= -1;
	mtlPass			= -1;
	depth			= -1.0f; 
}

}	// Render
