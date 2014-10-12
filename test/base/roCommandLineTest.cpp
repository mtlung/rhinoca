#include "pch.h"
#include "../../roar/base/roCommandLine.h"

using namespace ro;

struct CommandLineTest {};

TEST_FIXTURE(CommandLineTest, test)
{
	String json;
	roParseCommandLine(
		"hello usage:\n"
		" ((a1 a2|a3)|a4)\n"
		" (a|b)\n"
		" ship <name> move <x> <y>\n"
		" ship mine (set|remove)\n"
		"Options:\n"
		" -h --help\n"
		" --version\n",
		" ship Titanic move 1 2", json
	);

	// Test data from:
	// https://github.com/docopt/docopt.cpp/blob/master/testcases.docopt
	const roUtf8* testData[][3] = {
//		{ "Usage:",						"",				"{}" },
//		{ "Usage:",						"--xxx",		"{}" },

		{ "Usage:\n <arg>",				"10 ",			"{\"arg\":\"10\"}" },
		{ "Usage:\n <arg>",				"10 20",		NULL },
		{ "Usage:\n <arg>",				"",				NULL },

		{ "Usage:\n [<arg>]",			"10",			"{\"arg\":\"10\"}" },
		{ "Usage:\n [<arg>]",			"10 20",		NULL },
		{ "Usage:\n [<arg>]",			"",				"{\"arg\":null}" },
	};

	for(roSize i=0; i<roCountof(testData); ++i) {
		String json;
		bool ret = roParseCommandLine(testData[i][0], testData[i][1], json);
		CHECK(((testData[i][2] != NULL) ^ ret) == 0);

		if(testData[i][2])
			CHECK_EQUAL(testData[i][2], json.c_str());
	}
}
