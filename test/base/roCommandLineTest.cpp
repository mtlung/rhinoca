#include "pch.h"
#include "../../roar/base/roCommandLine.h"

using namespace ro;

struct CommandLineTest {};

TEST_FIXTURE(CommandLineTest, test)
{
	// Test data from:
	// https://github.com/docopt/docopt.cpp/blob/master/testcases.docopt
	const roUtf8* testData[][3] = {
//		{ "Usage:",													"",				"{}" },
//		{ "Usage:",													"--xxx",		"{}" },

		{ "Options:\n -p PATH, --path=<path>  Path to files [default: ./]",		"",		"" },
		{ "Options:\n -p PATH  Path to files [default: ./]",		"",		"" },

		{ "Usage:\n (<kind> --all | <name>)\nOptions:\n -p PATH  Path to files [default: ./]",		"10 --all",		"{\"kind\":\"10\",\"--all\":\"true\",\"name\":\"\"}" },

		{ "Usage:\n [<kind> | (<name> <type>)]",					"20 40",		"{\"kind\":\"\",\"name\":\"20\",\"type\":\"40\"}" },

		{ "Usage:\n <arg>",											"10",			"{\"arg\":\"10\"}" },
		{ "Usage:\n <arg>",											"10 20",		NULL },
		{ "Usage:\n <arg>",											"",				NULL },

		{ "Usage:\n [<arg>]",										"10",			"{\"arg\":\"10\"}" },
		{ "Usage:\n [<arg>]",										"10 20",		NULL },
		{ "Usage:\n [<arg>]",										"",				"{\"arg\":null}" },

		{ "Usage:\n <kind> <name> <type>",							"10 20 40",		"{\"kind\":\"10\",\"name\":\"20\",\"type\":\"40\"}" },
		{ "Usage:\n <kind> <name> <type>",							"10 20",		NULL },
		{ "Usage:\n <kind> <name> <type>",							"",				NULL },

		{ "Usage:\n <kind> [<name> <type>]",						"10 20 40",		"{\"kind\":\"10\",\"name\":\"20\",\"type\":\"40\"}" },
		{ "Usage:\n <kind> [<name> <type>]",						"10 20",		"{\"kind\":\"10\",\"name\":\"20\",\"type\":\"\"}" },

		{ "Usage:\n [<kind> | (<name> <type>)]",					"10 20 40",		NULL },
		{ "Usage:\n [<kind> | (<name> <type>)]",					"20 40",		"{\"kind\":\"\",\"name\":\"20\",\"type\":\"40\"}" },
		{ "Usage:\n [<kind> | (<name> <type>)]",					"",				"{\"kind\":\"\",\"name\":\"\",\"type\":\"\"}" },

		{ "Usage:\n (<kind> --all | <name>)\nOptions:\n --all",		"10 --all",		"{\"kind\":\"10\",\"--all\":\"true\",\"name\":\"\"}" },
		{ "Usage:\n (<kind> --all | <name>)\nOptions:\n --all",		"10",			"{\"kind\":\"\",\"--all\":\"false\",\"name\":\"10\"}" },
		{ "Usage:\n (<kind> --all | <name>)\nOptions:\n --all",		"",				NULL },

		// POSIXly correct tokenization
		{ "usage:\n [<input file>]",								"f.txt",		"{\"input file\":\"f.txt\"}" },
	};

	for(roSize i=0; i<roCountof(testData); ++i) {
		String json;
		bool ret = roParseCommandLine(testData[i][0], testData[i][1], json);
		CHECK(((testData[i][2] != NULL) ^ ret) == 0);

		if(testData[i][2])
			CHECK_EQUAL(testData[i][2], json.c_str());
	}
}
