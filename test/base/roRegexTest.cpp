#include "pch.h"
#include "../../roar/base/roRegex.h"

using namespace ro;

struct RegexTest {};

// strScan(str, "http://(:i)\.(com|org)(\::z)?((/:i)*)", domain, ending, port, path);
TEST_FIXTURE(RegexTest, test)
{
	const roUtf8* testData[][4] = {
		{ "^(b+|a){1,2}c",			"",		"abbac",											"bc`b" },

		{ "(a)*b",											"",		"aab",											"aab`a" },
		{ "(abc)d",					"",		"abcd",											"abcd`abc" },

		{ "^(b+|a){1,2}c",			"",		"bc",											"bc`b" },

		{ "a|b|c",					"",		"a",											"a" },
		{ "[a-c]*d",				"",		"ad",											"ad" },
		{ "a{1,2}",					"",		"a",											"a" },
		{ "[abc]d",					"",		"bd",											"bd" },
		{ "(a)?b",					"",		"b",											"b`" },
		{ "a?b",					"",		"b",											"b" },
		{ "(a*)*b",					"",		"b",											"b`" },
		{ "(a)*b",					"",		"aab",											"aab`a" },
		{ "(a)*b",					"",		"ab",											"ab`a" },
		{ "(a*)b",					"",		"ab",											"ab`a" },
		{ "(a)b",					"",		"ab",											"ab`a" },
		{ "a*b",					"",		"ab",											"ab" },
		{ "a*",						"",		"aa",											"aa" },
		{ "abc",					"",		"abc",											"abc" },

		{ "^(b+|a){1,2}c",			"",		"bc",											"bc`b" },

		{ "^xxx[0-9]*$",			"",		"xxx",											"xxx" },
		{ "the quick brown fox",	"",		"the quick brown fox",							"the quick brown fox" },
		{ NULL,						"",		"the quick brown FOX",							NULL },
		{ NULL,						"",		"What do you know about the quick brown fox?",	"the quick brown fox" },
		{ NULL,						"",		"What do you know about THE QUICK BROWN FOX?",	NULL },
		{ "The quick brown fox",	"i",	"the quick brown fox",							"the quick brown fox" },
		{ NULL,						"i",	"The quick brown FOX",							"The quick brown FOX" },
		{ NULL,						"i",	"What do you know about the quick brown fox?",	"the quick brown fox" },
		{ NULL,						"i",	"What do you know about THE QUICK BROWN FOX?",	"THE QUICK BROWN FOX" },

		{ "a*abc?xyz+pqr{3}ab{2,}xy{4,5}pq{0,6}AB{0,}zz",	"",		"abxyzpqrrrabbxyyyypqAzz",						"abxyzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aabxyzpqrrrabbxyyyypqAzz",						"aabxyzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aaabxyzpqrrrabbxyyyypqAzz",					"aaabxyzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aaaabxyzpqrrrabbxyyyypqAzz",					"aaaabxyzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"abcxyzpqrrrabbxyyyypqAzz",						"abcxyzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aabcxyzpqrrrabbxyyyypqAzz",					"aabcxyzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aaabcxyzpqrrrabbxyyyypAzz",					"aaabcxyzpqrrrabbxyyyypAzz" },
		{ NULL,												"",		"aaabcxyzpqrrrabbxyyyypqAzz",					"aaabcxyzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aaabcxyzpqrrrabbxyyyypqqAzz",					"aaabcxyzpqrrrabbxyyyypqqAzz" },
		{ NULL,												"",		"aaabcxyzpqrrrabbxyyyypqqqAzz",					"aaabcxyzpqrrrabbxyyyypqqqAzz" },
		{ NULL,												"",		"aaabcxyzpqrrrabbxyyyypqqqqAzz",				"aaabcxyzpqrrrabbxyyyypqqqqAzz" },
		{ NULL,												"",		"aaabcxyzpqrrrabbxyyyypqqqqqAzz",				"aaabcxyzpqrrrabbxyyyypqqqqqAzz" },
		{ NULL,												"",		"aaabcxyzpqrrrabbxyyyypqqqqqqAzz",				"aaabcxyzpqrrrabbxyyyypqqqqqqAzz" },
		{ NULL,												"",		"aaaabcxyzpqrrrabbxyyyypqAzz",					"aaaabcxyzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"abxyzzpqrrrabbxyyyypqAzz",						"abxyzzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aabxyzzzpqrrrabbxyyyypqAzz",					"aabxyzzzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aaabxyzzzzpqrrrabbxyyyypqAzz",					"aaabxyzzzzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aaaabxyzzzzpqrrrabbxyyyypqAzz",				"aaaabxyzzzzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"abcxyzzpqrrrabbxyyyypqAzz",					"abcxyzzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aabcxyzzzpqrrrabbxyyyypqAzz",					"aabcxyzzzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aaabcxyzzzzpqrrrabbxyyyypqAzz",				"aaabcxyzzzzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aaaabcxyzzzzpqrrrabbxyyyypqAzz",				"aaaabcxyzzzzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"aaaabcxyzzzzpqrrrabbbxyyyypqAzz",				"aaaabcxyzzzzpqrrrabbbxyyyypqAzz" },
		{ NULL,												"",		"aaaabcxyzzzzpqrrrabbbxyyyyypqAzz",				"aaaabcxyzzzzpqrrrabbbxyyyyypqAzz" },
		{ NULL,												"",		"aaabcxyzpqrrrabbxyyyypABzz",					"aaabcxyzpqrrrabbxyyyypABzz" },
		{ NULL,												"",		"aaabcxyzpqrrrabbxyyyypABBzz",					"aaabcxyzpqrrrabbxyyyypABBzz" },
//		{ NULL,												"",		">>>aaabxyzpqrrrabbxyyyypqAzz",					"aaabxyzpqrrrabbxyyyypqAzz" },
//		{ NULL,												"",		">aaaabxyzpqrrrabbxyyyypqAzz",					"aaaabxyzpqrrrabbxyyyypqAzz" },
//		{ NULL,												"",		">>>>abcxyzpqrrrabbxyyyypqAzz",					"abcxyzpqrrrabbxyyyypqAzz" },
		{ NULL,												"",		"*** Failers",									NULL },
		{ NULL,												"",		"abxyzpqrrabbxyyyypqAzz",						NULL },
		{ NULL,												"",		"abxyzpqrrrrabbxyyyypqAzz",						NULL },

		{ "^(abc){1,2}zz",			"",		"abczz",										"abczz`abc" },
		{ NULL,						"",		"abcabczz",										"abcabczz`abc" },
		{ NULL,						"",		"*** Failers",									NULL },
		{ NULL,						"",		"zz",											NULL },
		{ NULL,						"",		"abcabcabczz",									NULL },
		{ NULL,						"",		">>abczz",										NULL },

//		{ "^(b+?|a){1,2}?c",		"",		"bc",											"bc`b" },	// NOTE: +? is lazy +

		{ "^(b+|a){1,2}c",			"",		"bc",											"bc`b" },
		{ NULL,						"",		"bbc",											"bbc`bb" },
		{ NULL,						"",		"bbbc",											"bbbc`bbb" },
		{ NULL,						"",		"bac",											"bac`a" },
		{ NULL,						"",		"bbac",											"bbac`a" },
		{ NULL,						"",		"aac",											"aac`a" },
		{ NULL,						"",		"abbbbbbbbbbbc",								"abbbbbbbbbbbc`bbbbbbbbbbb" },
		{ NULL,						"",		"bbbbbbbbbbbac",								"bbbbbbbbbbbac`a" },
		{ NULL,						"",		"*** Failers",									NULL },
		{ NULL,						"",		"aaac",											NULL },
		{ NULL,						"",		"abbbbbbbbbbbac",								NULL },

		{ "^[ab\\]cde]",			"",		"athing",										"a" },
		{ NULL,						"",		"bthing",										"b" },
		{ NULL,						"",		"]thing",										"]" },
		{ NULL,						"",		"cthing",										"c" },
		{ NULL,						"",		"dthing",										"d" },
		{ NULL,						"",		"ething",										"e" },
		{ NULL,						"",		"*** Failers",									NULL },
		{ NULL,						"",		"fthing",										NULL },
		{ NULL,						"",		"[thing",										NULL },
		{ NULL,						"",		"\\thing",										NULL },

		{ "^[]cde]",				"",		"]thing",										"]" },
		{ NULL,						"",		"cthing",										"c" },
		{ NULL,						"",		"dthing",										"d" },
		{ NULL,						"",		"ething",										"e" },
		{ NULL,						"",		"*** Failers",									NULL },
		{ NULL,						"",		"athing",										NULL },
		{ NULL,						"",		"fthing",										NULL },

		{ "^[^ab\\]cde]",			"",		"fthing",										"f" },
		{ NULL,						"",		"[thing",										"[" },
		{ NULL,						"",		"\\thing",										"\\" },
		{ NULL,						"",		"*** Failers",									"*" },
		{ NULL,						"",		"athing",										NULL },
		{ NULL,						"",		"bthing",										NULL },
		{ NULL,						"",		"]thing",										NULL },
		{ NULL,						"",		"cthing",										NULL },
		{ NULL,						"",		"dthing",										NULL },
		{ NULL,						"",		"ething",										NULL },

		{ "^[^]cde]",				"",		"athing",										"a" },
		{ NULL,						"",		"fthing",										"f" },
		{ NULL,						"",		"*** Failers",									"*" },
		{ NULL,						"",		"]thing",										NULL },
		{ NULL,						"",		"cthing",										NULL },
		{ NULL,						"",		"dthing",										NULL },
		{ NULL,						"",		"ething",										NULL },

		{ "^[0-9]+$",				"",		"0",											"0" },
		{ NULL,						"",		"1",											"1" },
		{ NULL,						"",		"2",											"2" },
		{ NULL,						"",		"3",											"3" },
		{ NULL,						"",		"4",											"4" },
		{ NULL,						"",		"5",											"5" },
		{ NULL,						"",		"6",											"6" },
		{ NULL,						"",		"7",											"7" },
		{ NULL,						"",		"8",											"8" },
		{ NULL,						"",		"9",											"9" },
		{ NULL,						"",		"10",											"10" },
		{ NULL,						"",		"100",											"100" },
		{ NULL,						"",		"*** Failers",									NULL },
		{ NULL,						"",		"abc",											NULL },

		{ "^xxx[0-9]+$",			"",		"xxx0",											"xxx0" },
		{ NULL,						"",		"xxx1234",										"xxx1234" },
		{ NULL,						"",		"*** Failers",									NULL },
		{ NULL,						"",		"xxx",											NULL },

		{ "^.+[0-9][0-9][0-9]$",	"",		"x123",											"x123" },

		{ "a?a?aa",					"",		"aa",											"aa" },

		{ "[a-c0-9]", "", "9", "9" },
		{ "[^1]", "", "abc", "a" },
		{ "(012|abc|lung)+",		"",		"abclung",										"abclung`lung" },
		{ "abc|def",	"i","Abc", "Abc" },
		{ "abc|ha|def", "",	"def", "def" },
		{ "abc|def",	"",	"abc", "abc" },
		{ "ab*",		"",	"ababc", "ab" },
	};

	Regex regex;

	const roUtf8* regStr = NULL;
	for(roSize i=0; i<roCountof(testData); ++i) {
		if(testData[i][0])
			regStr = testData[i][0];
		regex.match(testData[i][2], regStr, testData[i][1]);

		const roUtf8* result = testData[i][3];
		if(result) {
			if(regex.result.isEmpty()) {
				CHECK_EQUAL("", regStr);
			}
			if(!regex.result.isEmpty()) {
				String str;
				for(roSize i=0; i<regex.result.size(); ++i) {
					str += regex.result[i];
					if(i != regex.result.size() - 1)
						str += "`";
				}
				CHECK_EQUAL(result, str.c_str());
			}
		}
		else
			CHECK(regex.result.isEmpty());
	}

	regex.match("", "ab*");
	regex.match("ababc", "ab*");
	regex.match("ab", "(abc)?ab");
	regex.match("http://yahoo.com", "http://{:i}\\.{com|org}(\\::z)?{(/:i)*}");
}
