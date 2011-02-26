#include "pch.h"
#include "../extension/swf/SWFParser.h"

TEST(SWFParsingTest)
{
	const char *path = "../../test/media/swf/snailrunner1.swf";
	//const char *path = "../../media/swf/whitebird1.swf";
	SWFParser parser;
	parser.Parse( path );
}
