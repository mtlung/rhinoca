#include "pch.h"
#include "../../roar/base/roCommandLine.h"

using namespace ro;

struct CommandLineTest {};

TEST_FIXTURE(CommandLineTest, test)
{
	// Test data from:
	// https://github.com/docopt/docopt.cpp/blob/master/testcases.docopt
	const roUtf8* testData[][3] = {
		// 0
		{ "Usage:",																			"",					"{}" },
		{ "Usage:",																			"--xxx",			NULL },

		// 2
		{ "Usage:\n [options]\nOptions:\n -a  All.",										"",					"{\"a\":false}" },
		{ "Usage:\n [options]\nOptions:\n -a  All.",										"-a",				"{\"a\":true}" },
		{ "Usage:\n [options]\nOptions:\n -a  All.",										"-x",				NULL },

		// 5
		{ "Usage:\n [options]\nOptions:\n --all  All.",										"",					"{\"all\":false}" },
		{ "Usage:\n [options]\nOptions:\n --all  All.",										"--all",			"{\"all\":true}" },
		{ "Usage:\n [options]\nOptions:\n --all  All.",										"--xxx",			NULL },

		// 8
		{ "Usage:\n [options]\nOptions:\n -v, --verbose  Verbose.",							"--verbose",		"{\"verbose\":true}" },
//		{ "Usage:\n [options]\nOptions:\n -v, --verbose  Verbose.",							"--ver",			"{}" },						// Not supported
		{ "Usage:\n [options]\nOptions:\n -v, --verbose  Verbose.",							"-v",				"{\"verbose\":true}" },

		// 10
		{ "Usage:\n [options]\nOptions:\n -p PATH",											"-p home/",			"{\"p\":\"home/\"}" },
		{ "Usage:\n [options]\nOptions:\n -p PATH",											"-phome/",			"{\"p\":\"home/\"}" },
		{ "Usage:\n [options]\nOptions:\n -p PATH",											"-p",				NULL },

		// 13
		{ "Usage:\n [options]\nOptions:\n --path <path>",									"--path home/",		"{\"path\":\"home/\"}" },
		{ "Usage:\n [options]\nOptions:\n --path <path>",									"--path=home/",		"{\"path\":\"home/\"}" },
//		{ "Usage:\n [options]\nOptions:\n --path <path>",									"--pa home/",		"{\"path\":\"home/\"}" },	// Not supported
//		{ "Usage:\n [options]\nOptions:\n --path <path>",									"--pa=home/",		"{\"path\":\"home/\"}" },	// Not supported
		{ "Usage:\n [options]\nOptions:\n --path <path>",									"--path",			NULL },

		// 16
		{ "Usage:\n [options]\nOptions:\n -p PATH, --path=<path> Path to files.",			"-proot",			"{\"path\":\"root\"}" },

		// 17
		{ "Usage:\n [options]\nOptions:\n -p --path PATH  Path to files.",					"-p root",			"{\"path\":\"root\"}" },
		{ "Usage:\n [options]\nOptions:\n -p --path PATH  Path to files.",					"--path root",		"{\"path\":\"root\"}" },

		// 19
		{ "Usage:\n [options]\nOptions:\n -p PATH  Path to files [default: ./]",			"",					"{\"p\":\"./\"}" },
		{ "Usage:\n [options]\nOptions:\n -p PATH  Path to files [default: ./]",			"-phome",			"{\"p\":\"home\"}" },

		// 21
		{ "UsAgE:\n [options]\nOpTiOnS:\n --path=<files>  Path to files [dEfAuLt: /root]",	"",					"{\"path\":\"/root\"}" },
		{ "UsAgE:\n [options]\nOpTiOnS:\n --path=<files>  Path to files [dEfAuLt: /root]",	"--path=home",		"{\"path\":\"home\"}" },

		// 23
		{ "usage:\n [options]\noptions:\n -a  Add\n -r  Remote\n -m <msg>  Message",		"-a -r -m Hello",	"{\"a\":true,\"r\":true,\"m\":\"Hello\"}" },
//		{ "usage:\n [options]\noptions:\n -a  Add\n -r  Remote\n -m <msg>  Message",		"-armyourass",		"{\"a\":true,\"r\":true,\"m\":\"yourass\"}" },	// Not supported yet
		{ "usage:\n [options]\noptions:\n -a  Add\n -r  Remote\n -m <msg>  Message",		"-a -r",			"{\"a\":true,\"r\":true,\"m\":null}" },

		// 25
		{ "Usage:\n [options]\nOptions:\n --version\n --verbose",							"--version",		"{\"version\":true,\"verbose\":false}" },
		{ "Usage:\n [options]\nOptions:\n --version\n --verbose",							"--verbose",		"{\"verbose\":true,\"version\":false}" },
		{ "Usage:\n [options]\nOptions:\n --version\n --verbose",							"--ver",			NULL },
//		{ "Usage:\n [options]\nOptions:\n --version\n --verbose",							"--verb",			"{\"version\":false,\"verbose\":true}" },	// Not supported

//		{ "Usage:\n [-a -r -m <msg>]\nOptions:\n -a  Add\n -r  Remote\n -m <msg>  Message",	"-armyourass",		"{\"a\":true,\"r\":true,\"m\":\"yourass\"}" },	// Not supported yet

		// 28
		{ "Usage:\n -a -b\nOptions:\n -a\n -b",												"-a -b",			"{\"a\":true,\"b\":true}" },
		{ "Usage:\n -a -b\nOptions:\n -a\n -b",												"-b -a",			"{\"a\":true,\"b\":true}" },
		{ "Usage:\n -a -b\nOptions:\n -a\n -b",												"-a",				NULL },
		{ "Usage:\n -a -b\nOptions:\n -a\n -b",												"",					NULL },

		// 32
		{ "Usage:\n (-a -b)\nOptions:\n -a\n -b",											"-a -b",			"{\"a\":true,\"b\":true}" },
		{ "Usage:\n (-a -b)\nOptions:\n -a\n -b",											"-b -a",			"{\"a\":true,\"b\":true}" },
		{ "Usage:\n (-a -b)\nOptions:\n -a\n -b",											"-a",				NULL },
		{ "Usage:\n (-a -b)\nOptions:\n -a\n -b",											"",					NULL },

		// 36
		{ "Usage:\n [-a] -b\nOptions:\n -a\n -b",											"-a -b",			"{\"a\":true,\"b\":true}" },
		{ "Usage:\n [-a] -b\nOptions:\n -a\n -b",											"-b -a",			"{\"a\":true,\"b\":true}" },
		{ "Usage:\n [-a] -b\nOptions:\n -a\n -b",											"-a",				NULL },
		{ "Usage:\n [-a] -b\nOptions:\n -a\n -b",											"-b",				"{\"b\":true,\"a\":false}" },
		{ "Usage:\n [-a] -b\nOptions:\n -a\n -b",											"",					NULL },

		// 41
		{ "Usage:\n [(-a -b)]\nOptions:\n -a\n -b",											"-a -b",			"{\"a\":true,\"b\":true}" },
		{ "Usage:\n [(-a -b)]\nOptions:\n -a\n -b",											"-b -a",			"{\"a\":true,\"b\":true}" },
		{ "Usage:\n [(-a -b)]\nOptions:\n -a\n -b",											"-a",				NULL },
		{ "Usage:\n [(-a -b)]\nOptions:\n -a\n -b",											"-b",				NULL },
		{ "Usage:\n [(-a -b)]\nOptions:\n -a\n -b",											"",					"{\"a\":false,\"b\":false}" },

		// 46
		{ "Usage:\n (-a|-b)\nOptions:\n -a\n -b",											"-a -b",			NULL },
		{ "Usage:\n (-a|-b)\nOptions:\n -a\n -b",											"",					NULL },
		{ "Usage:\n (-a|-b)\nOptions:\n -a\n -b",											"-a",				"{\"a\":true,\"b\":false}" },
		{ "Usage:\n (-a|-b)\nOptions:\n -a\n -b",											"-b",				"{\"b\":true,\"a\":false}" },

		// 50
		{ "Usage:\n <arg>",																	"10",				"{\"arg\":\"10\"}" },
		{ "Usage:\n <arg>",																	"10 20",			NULL },
		{ "Usage:\n <arg>",																	"",					NULL },

		// 53
		{ "Usage:\n [<arg>]",																"10",				"{\"arg\":\"10\"}" },
		{ "Usage:\n [<arg>]",																"10 20",			NULL },
		{ "Usage:\n [<arg>]",																"",					"{\"arg\":null}" },

		// 56
		{ "Usage:\n <kind> <name> <type>",													"10 20 40",			"{\"kind\":\"10\",\"name\":\"20\",\"type\":\"40\"}" },
		{ "Usage:\n <kind> <name> <type>",													"10 20",			NULL },
		{ "Usage:\n <kind> <name> <type>",													"",					NULL },

		// 59
		{ "Usage:\n <kind> [<name> <type>]",												"10 20 40",			"{\"kind\":\"10\",\"name\":\"20\",\"type\":\"40\"}" },
//		{ "Usage:\n <kind> [<name> <type>]",												"10 20",			"{\"kind\":\"10\",\"name\":\"20\",\"type\":null}" },
		{ "Usage:\n <kind> [<name>] [<type>]",												"10 20",			"{\"kind\":\"10\",\"name\":\"20\",\"type\":null}" },
		{ "Usage:\n <kind> [<name> <type>]",												"",					NULL },

		// 62
		{ "Usage:\n [<kind> | (<name> <type>)]",											"10 20 40",			NULL },
//		{ "Usage:\n [<kind> | (<name> <type>)]",											"20 40",			"{\"kind\":null,\"name\":\"20\",\"type\":\"40\"}" },	// Alternative ordering problem
		{ "Usage:\n [<kind> | (<name> <type>)]",											"",					"{\"kind\":null,\"name\":null,\"type\":null}" },

		// 64
		{ "Usage:\n (<kind> --all | <name>)\nOptions:\n --all",								"10 --all",			"{\"all\":true,\"kind\":\"10\",\"name\":null}" },
		{ "Usage:\n (<kind> --all | <name>)\nOptions:\n --all",								"10",				"{\"all\":false,\"kind\":null,\"name\":\"10\"}" },
		{ "Usage:\n (<kind> --all | <name>)\nOptions:\n --all",								"",					NULL },

/*
		{ "Usage:\n [-armmsg]\nOptions:\n -a  Add\n -r  Remote\n -m <msg>  Message",		"-a -r -m Hello",	"{}" },


		{ "Usage:\n (<kind> --all | <name>)\nOptions:\n -p PATH  Path to files [default: ./]",		"10 --all",		"{\"kind\":\"10\",\"--all\":\"true\",\"name\":\"\"}" },


		{ "Usage:\n (<kind> --all | <name>)\nOptions:\n --all",		"10 --all",		"{\"kind\":\"10\",\"--all\":\"true\",\"name\":\"\"}" },
		{ "Usage:\n (<kind> --all | <name>)\nOptions:\n --all",		"10",			"{\"kind\":\"\",\"--all\":\"false\",\"name\":\"10\"}" },
		{ "Usage:\n (<kind> --all | <name>)\nOptions:\n --all",		"",				NULL },

		// POSIXly correct tokenization
		{ "usage:\n [<input file>]",								"f.txt",		"{\"input file\":\"f.txt\"}" },*/
	};

	for(roSize i=65; i<roCountof(testData); ++i) {
		String json;
		bool ret = roParseCommandLine(testData[i][0], testData[i][1], json);
		CHECK(((testData[i][2] != NULL) ^ ret) == 0);

		if(testData[i][2])
			CHECK_EQUAL(testData[i][2], json.c_str());

		if(json != testData[i][2])
			json = json;
	}
}
