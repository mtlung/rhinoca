#include "pch.h"
#include "../../roar/base/roRegex.h"

using namespace ro;

struct RegexTest {};

// strScan(str, "http://(:i)\.(com|org)(\::z)?((/:i)*)", domain, ending, port, path);
TEST_FIXTURE(RegexTest, test)
{
	const roUtf8* testData[][4] = {
		{ "<.*>",						"",		"<img>def</img>",								"<img>def</img>" },
		{ "<.*?>",						"",		"<img>def</img>",								"<img>" },

		{ "(a)*b",						"",		"aab",											"aab`a" },
		{ "(abc)d",						"",		"abcd",											"abcd`abc" },
		{ "(?:(A|a)(B|b))+",			"",		"ABa",											"AB`A`B" },
		{ "(((a)?b)?c)?",				"",		"abc",											"abc`abc`ab`a" },
		{ "(?:(?:(a) (b)? c)?)?",		"",		"a b c",										"a b c`a`b" },
		{ "(a)(b)|(a)(c)",				"",		"ac",											"ac```a`c" },

		{ "[^ a-z]+",					"",		"path",											NULL },
		{ "[^ a-z]+",					"",		"PATH",											"PATH" },

		{ "a b",						"",		"a",											NULL },
		{ "a|b|c",						"",		"a",											"a" },
		{ "b| a|[^ ]*|",				"",		" a",											" a" },
		{ "[^ ]*| a",					"",		" a",											"" },	// Note that alternation is evaluated left to right, resulting a valid but empty result
		{ "[a-c]*d",					"",		"ad",											"ad" },
		{ "a{1,2}",						"",		"a",											"a" },
		{ "[abc]d",						"",		"bd",											"bd" },
		{ "(a)?b",						"",		"b",											"b`" },
		{ "a?b",						"",		"b",											"b" },
		{ "(a*)*b",						"",		"b",											"b`" },
		{ "(a)*b",						"",		"aab",											"aab`a" },
		{ "(a)*b",						"",		"ab",											"ab`a" },
		{ "(a*)b",						"",		"ab",											"ab`a" },
		{ "(a)b",						"",		"ab",											"ab`a" },
		{ "a*b",						"",		"ab",											"ab" },
		{ "a*",							"",		"aa",											"aa" },
		{ "abc",						"",		"abc",											"abc" },

		{ "^xxx[0-9]*$",				"",		"xxx",											"xxx" },
		{ "the quick brown fox",		"",		"the quick brown fox",							"the quick brown fox" },
		{ NULL,							"",		"the quick brown FOX",							NULL },
		{ NULL,							"",		"What do you know about the quick brown fox?",	"the quick brown fox" },
		{ NULL,							"",		"What do you know about THE QUICK BROWN FOX?",	NULL },
		{ "The quick brown fox",		"i",	"the quick brown fox",							"the quick brown fox" },
		{ NULL,							"i",	"The quick brown FOX",							"The quick brown FOX" },
		{ NULL,							"i",	"What do you know about the quick brown fox?",	"the quick brown fox" },
		{ NULL,							"i",	"What do you know about THE QUICK BROWN FOX?",	"THE QUICK BROWN FOX" },

		{ "a*abc?xyz+pqr{3}ab{2,}xy{4,5}pq{0,6}AB{0,}zz",	"",	"abxyzpqrrrabbxyyyypqAzz",		"abxyzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aabxyzpqrrrabbxyyyypqAzz",						"aabxyzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aaabxyzpqrrrabbxyyyypqAzz",					"aaabxyzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aaaabxyzpqrrrabbxyyyypqAzz",					"aaaabxyzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"abcxyzpqrrrabbxyyyypqAzz",						"abcxyzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aabcxyzpqrrrabbxyyyypqAzz",					"aabcxyzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aaabcxyzpqrrrabbxyyyypAzz",					"aaabcxyzpqrrrabbxyyyypAzz" },
		{ NULL,							"",		"aaabcxyzpqrrrabbxyyyypqAzz",					"aaabcxyzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aaabcxyzpqrrrabbxyyyypqqAzz",					"aaabcxyzpqrrrabbxyyyypqqAzz" },
		{ NULL,							"",		"aaabcxyzpqrrrabbxyyyypqqqAzz",					"aaabcxyzpqrrrabbxyyyypqqqAzz" },
		{ NULL,							"",		"aaabcxyzpqrrrabbxyyyypqqqqAzz",				"aaabcxyzpqrrrabbxyyyypqqqqAzz" },
		{ NULL,							"",		"aaabcxyzpqrrrabbxyyyypqqqqqAzz",				"aaabcxyzpqrrrabbxyyyypqqqqqAzz" },
		{ NULL,							"",		"aaabcxyzpqrrrabbxyyyypqqqqqqAzz",				"aaabcxyzpqrrrabbxyyyypqqqqqqAzz" },
		{ NULL,							"",		"aaaabcxyzpqrrrabbxyyyypqAzz",					"aaaabcxyzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"abxyzzpqrrrabbxyyyypqAzz",						"abxyzzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aabxyzzzpqrrrabbxyyyypqAzz",					"aabxyzzzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aaabxyzzzzpqrrrabbxyyyypqAzz",					"aaabxyzzzzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aaaabxyzzzzpqrrrabbxyyyypqAzz",				"aaaabxyzzzzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"abcxyzzpqrrrabbxyyyypqAzz",					"abcxyzzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aabcxyzzzpqrrrabbxyyyypqAzz",					"aabcxyzzzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aaabcxyzzzzpqrrrabbxyyyypqAzz",				"aaabcxyzzzzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aaaabcxyzzzzpqrrrabbxyyyypqAzz",				"aaaabcxyzzzzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"aaaabcxyzzzzpqrrrabbbxyyyypqAzz",				"aaaabcxyzzzzpqrrrabbbxyyyypqAzz" },
		{ NULL,							"",		"aaaabcxyzzzzpqrrrabbbxyyyyypqAzz",				"aaaabcxyzzzzpqrrrabbbxyyyyypqAzz" },
		{ NULL,							"",		"aaabcxyzpqrrrabbxyyyypABzz",					"aaabcxyzpqrrrabbxyyyypABzz" },
		{ NULL,							"",		"aaabcxyzpqrrrabbxyyyypABBzz",					"aaabcxyzpqrrrabbxyyyypABBzz" },
		{ NULL,							"",		">>>aaabxyzpqrrrabbxyyyypqAzz",					"aaabxyzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		">aaaabxyzpqrrrabbxyyyypqAzz",					"aaaabxyzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		">>>>abcxyzpqrrrabbxyyyypqAzz",					"abcxyzpqrrrabbxyyyypqAzz" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"abxyzpqrrabbxyyyypqAzz",						NULL },
		{ NULL,							"",		"abxyzpqrrrrabbxyyyypqAzz",						NULL },

		{ "^(abc){1,2}zz",				"",		"abczz",										"abczz`abc" },
		{ NULL,							"",		"abcabczz",										"abcabczz`abc" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"zz",											NULL },
		{ NULL,							"",		"abcabcabczz",									NULL },
		{ NULL,							"",		">>abczz",										NULL },

		{ "^(b+?|a){1,2}?c",			"",		"bc",											"bc`b" },	// NOTE: +? is lazy +
		{ NULL,							"",		"bbc",											"bbc`b" },
		{ NULL,							"",		"bbbc",											"bbbc`bb" },
		{ NULL,							"",		"bac",											"bac`a" },
		{ NULL,							"",		"bbac",											"bbac`a" }, // assertion
		{ NULL,							"",		"aac",											"aac`a" },
		{ NULL,							"",		"abbbbbbbbbbbc",								"abbbbbbbbbbbc`bbbbbbbbbbb" },
		{ NULL,							"",		"bbbbbbbbbbbac",								"bbbbbbbbbbbac`a" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"aaac",											NULL },
		{ NULL,							"",		"abbbbbbbbbbbac",								NULL },

		{ "^(b+|a){1,2}c",				"",		"bc",											"bc`b" },
		{ NULL,							"",		"bbc",											"bbc`bb" },
		{ NULL,							"",		"bbbc",											"bbbc`bbb" },
		{ NULL,							"",		"bac",											"bac`a" },
		{ NULL,							"",		"bbac",											"bbac`a" },
		{ NULL,							"",		"aac",											"aac`a" },
		{ NULL,							"",		"abbbbbbbbbbbc",								"abbbbbbbbbbbc`bbbbbbbbbbb" },
		{ NULL,							"",		"bbbbbbbbbbbac",								"bbbbbbbbbbbac`a" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"aaac",											NULL },
		{ NULL,							"",		"abbbbbbbbbbbac",								NULL },

		{ "^(b+|a){1,2}?bc",			"",		"bbc",											"bbc`b" },

		{ "^(b*|ba){1,2}?bc",			"",		"babc",											"babc`ba" },
		{ NULL,							"",		"bbabc",										"bbabc`ba" },
		{ NULL,							"",		"bababc",										"bababc`ba" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"bababbc",										NULL },
		{ NULL,							"",		"babababc",										NULL },

		{ "^(ba|b*){1,2}?bc",			"",		"babc",											"babc`ba" },
		{ NULL,							"",		"bbabc",										"bbabc`ba" },
		{ NULL,							"",		"bababc",										"bababc`ba" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"bababbc",										NULL },
		{ NULL,							"",		"babababc",										NULL },

//		{ "\\ca\\cA\\c[\\c{\\c:",		"",		"\\x01\\x01\\e;z",								"\\x01\\x01\\x1b;z" },

		{ "^[ab\\]cde]",				"",		"athing",										"a" },
		{ NULL,							"",		"bthing",										"b" },
		{ NULL,							"",		"]thing",										"]" },
		{ NULL,							"",		"cthing",										"c" },
		{ NULL,							"",		"dthing",										"d" },
		{ NULL,							"",		"ething",										"e" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"fthing",										NULL },
		{ NULL,							"",		"[thing",										NULL },
		{ NULL,							"",		"\\thing",										NULL },

		{ "^[]cde]",					"",		"]thing",										"]" },
		{ NULL,							"",		"cthing",										"c" },
		{ NULL,							"",		"dthing",										"d" },
		{ NULL,							"",		"ething",										"e" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"athing",										NULL },
		{ NULL,							"",		"fthing",										NULL },

		{ "^[^ab\\]cde]",				"",		"fthing",										"f" },
		{ NULL,							"",		"[thing",										"[" },
		{ NULL,							"",		"\\thing",										"\\" },
		{ NULL,							"",		"*** Failers",									"*" },
		{ NULL,							"",		"athing",										NULL },
		{ NULL,							"",		"bthing",										NULL },
		{ NULL,							"",		"]thing",										NULL },
		{ NULL,							"",		"cthing",										NULL },
		{ NULL,							"",		"dthing",										NULL },
		{ NULL,							"",		"ething",										NULL },

		{ "^[^]cde]",					"",		"athing",										"a" },
		{ NULL,							"",		"fthing",										"f" },
		{ NULL,							"",		"*** Failers",									"*" },
		{ NULL,							"",		"]thing",										NULL },
		{ NULL,							"",		"cthing",										NULL },
		{ NULL,							"",		"dthing",										NULL },
		{ NULL,							"",		"ething",										NULL },

//		{ "^\\",						"",		"",												"\\x81" },

//		{ "^ÿ",							"",		"ÿ",											"\\xff" },

		{ "^[0-9]+$",					"",		"0",											"0" },
		{ NULL,							"",		"1",											"1" },
		{ NULL,							"",		"2",											"2" },
		{ NULL,							"",		"3",											"3" },
		{ NULL,							"",		"4",											"4" },
		{ NULL,							"",		"5",											"5" },
		{ NULL,							"",		"6",											"6" },
		{ NULL,							"",		"7",											"7" },
		{ NULL,							"",		"8",											"8" },
		{ NULL,							"",		"9",											"9" },
		{ NULL,							"",		"10",											"10" },
		{ NULL,							"",		"100",											"100" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"abc",											NULL },

		{ "^.*nter",					"",		"enter",										"enter" },
		{ NULL,							"",		"inter",										"inter" },
		{ NULL,							"",		"uponter",										"uponter" },

		{ "^xxx[0-9]+$",				"",		"xxx0",											"xxx0" },
		{ NULL,							"",		"xxx1234",										"xxx1234" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"xxx",											NULL },

		{ "^.+[0-9][0-9][0-9]$",		"",		"x123",											"x123" },
		{ NULL,							"",		"xx123",										"xx123" },
		{ NULL,							"",		"123456",										"123456" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"123",											NULL },
		{ NULL,							"",		"x1234",										"x1234" },

		{ "^.+?[0-9][0-9][0-9]$",		"",		"x123",											"x123" },
		{ NULL,							"",		"xx123",										"xx123" },
		{ NULL,							"",		"123456",										"123456" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"123",											NULL },
		{ NULL,							"",		"x1234",										"x1234" },

		{ "^([^!]+)!(.+)=apquxz\\.ixr\\.zzz\\.ac\\.uk$",	"",		"",							NULL },
		{ NULL,							"",		"abc!pqr=apquxz.ixr.zzz.ac.uk",					"abc!pqr=apquxz.ixr.zzz.ac.uk`abc`pqr" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"!pqr=apquxz.ixr.zzz.ac.uk",					NULL },
		{ NULL,							"",		"abc!=apquxz.ixr.zzz.ac.uk",					NULL },
		{ NULL,							"",		"abc!pqr=apquxz:ixr.zzz.ac.uk",					NULL },
		{ NULL,							"",		"abc!pqr=apquxz.ixr.zzz.ac.ukk",				NULL },

		{ ":",							"",		"Well, we need a colon: somewhere",				":" },
		{ NULL,							"",		"*** Fail if we don't",							NULL },

		{ "([\\da-f:]+)$",				"i",	"0abc",											"0abc`0abc" },
		{ NULL,							"i",	"abc",											"abc`abc" },
		{ NULL,							"i",	"fed",											"fed`fed" },
		{ NULL,							"i",	"E",											"E`E" },
		{ NULL,							"i",	"::",											"::`::" },
		{ NULL,							"i",	"5f03:12C0::932e",								"5f03:12C0::932e`5f03:12C0::932e" },
		{ NULL,							"i",	"fed def",										"def`def" },
		{ NULL,							"i",	"Any old stuff",								"ff`ff" },
		{ NULL,							"i",	"*** Failers",									NULL },
		{ NULL,							"i",	"0zzz",											NULL },
		{ NULL,							"i",	"gzzz",											NULL },
		{ NULL,							"i",	"fed\x20",										NULL },
		{ NULL,							"i",	"Any old rubbish",								NULL },

		{ "^.*\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})$",	"",		"",							NULL },
		{ NULL,							"",		".1.2.3",										".1.2.3`1`2`3" },
		{ NULL,							"",		"A.12.123.0",									"A.12.123.0`12`123`0" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		".1.2.3333",									NULL },
		{ NULL,							"",		"1.2.3",										NULL },
		{ NULL,							"",		"1234.2.3",										NULL },

		{ "^(\\d+)\\s+IN\\s+SOA\\s+(\\S+)\\s+(\\S+)\\s*\\(\\s*$",	"",		"",					NULL },
		{ NULL,							"",		"1 IN SOA non-sp1 non-sp2(",					"1 IN SOA non-sp1 non-sp2(`1`non-sp1`non-sp2" },
		{ NULL,							"",		"1    IN    SOA    non-sp1    non-sp2   (",		"1    IN    SOA    non-sp1    non-sp2   (`1`non-sp1`non-sp2" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"1IN SOA non-sp1 non-sp2(",						NULL },

		{ "^[a-zA-Z\\d][a-zA-Z\\d\\-]*(\\.[a-zA-Z\\d][a-zA-z\\d\\-]*)*\\.$",	"",	"",			NULL },
		{ NULL,							"",		"a.",											"a.`" },
		{ NULL,							"",		"Z.",											"Z.`" },
		{ NULL,							"",		"2.",											"2.`" },
		{ NULL,							"",		"ab-c.pq-r.",									"ab-c.pq-r.`.pq-r" },
		{ NULL,							"",		"sxk.zzz.ac.uk.",								"sxk.zzz.ac.uk.`.uk" },
		{ NULL,							"",		"x-.y-.",										"x-.y-.`.y-" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"-abc.peq.",									NULL },

		{ "^\\*\\.[a-z]([a-z\\-\\d]*[a-z\\d]+)?(\\.[a-z]([a-z\\-\\d]*[a-z\\d]+)?)*$","","",		NULL },
		{ NULL,							"",		"*.a",											"*.a```" },
		{ NULL,							"",		"*.b0-a",										"*.b0-a`0-a``" },
		{ NULL,							"",		"*.c3-b.c",										"*.c3-b.c`3-b`.c`" },
		{ NULL,							"",		"*.c-a.b-c",									"*.c-a.b-c`-a`.b-c`-c" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"*.0",											NULL },
		{ NULL,							"",		"*.a-",											NULL },
		{ NULL,							"",		"*.a-b.c-",										NULL },
		{ NULL,							"",		"*.c-a.0-c",									NULL },

//		{ "^(?=ab(de))(abd)(e)",		"",		"abde",											"abde`de`abd`e" },

//		{ "^(?!(ab)de|x)(abd)(f)",		"",		"abdf",											"abdf``abd`f" },

//		{ "^(?=(ab(cd)))(ab)",			"",		"abcd",											"abcd`ab`abcd`ab" },

		{ "^[\\da-f](\\.[\\da-f])*$",	"i",	"a.b.c.d",										"a.b.c.d`.d" },
		{ NULL,							"i",	"A.B.C.D",										"A.B.C.D`.D" },
		{ NULL,							"i",	"a.b.c.1.2.3.C",								"a.b.c.1.2.3.C`.C" },

		{ "^\\\".*\\\"\\s*(;.*)?$",		"",		"\"1234\"",										"\"1234\"`" },
		{ NULL,							"",		"\"abcd\" ;",									"\"abcd\" ;`;" },
		{ NULL,							"",		"\"\" ; rhubarb",								"\"\" ; rhubarb`; rhubarb" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"\"1234\" : things",							NULL },

//		{ "   ^    a   (?# begins with a)  b\\sc (?# then b c) $ (?# then end)", "", "",		"" },	// ?# comment
//		{ NULL,							"",		"ab c",											"ab c`ab c" },
//		{ NULL,							"",		"*** Failers",									NULL },
//		{ NULL,							"",		"abc",											NULL },
//		{ NULL,							"",		"ab cde",										NULL },

//		{ "(?x)   ^    a   (?# begins with a)  b\\sc (?# then b c) $ (?# then end)", "", "",	"" },
//		{ NULL,							"",		"ab c",											"ab c`ab c" },
//		{ NULL,							"",		"*** Failers",									NULL },
//		{ NULL,							"",		"abc",											NULL },
//		{ NULL,							"",		"ab cde",										NULL },

//		{ "   a\ b[c ]d       $",		"x",	"a bcd",										"a bcd" },	// Free spacing mode
//		{ NULL,							"x",	"a b d",										"a b d" },
//		{ NULL,							"x",	"*** Failers",									NULL },
//		{ NULL,							"x",	"abcd",											NULL },
//		{ NULL,							"x",	"ab d",											NULL },

		{ "^(a(b(c)))(d(e(f)))(h(i(j)))(k(l(m)))$",	"",	"",										NULL },
		{ NULL,							"",		"abcdefhijklm",									"abcdefhijklm`abc`bc`c`def`ef`f`hij`ij`j`klm`lm`m" },

		{ "^(?:a(b(c)))(?:d(e(f)))(?:h(i(j)))(?:k(l(m)))$",	"",	"",								NULL },
		{ NULL,							"",		"abcdefhijklm",									"abcdefhijklm`bc`c`ef`f`ij`j`lm`m" },

//		{ "^[\\w][\\W][\\s][\\S][\\d][\\D][\\b][\\n][\\c]][\\022]",	"",	"",						NULL },
//		{ NULL,							"",		"a+ Z0+\\x08\n\\x1d\\x12",						"a+ Z0+\\x08\\x0a\\x1d\\x12" },

		{ "^[.^$|()*+?{,}]+",			"",		".^$(*+)|{?,?}",								".^$(*+)|{?,?}" },

		{ "^a*\\w",						"",		"z",											"z" },
		{ NULL,							"",		"az",											"az" },
		{ NULL,							"",		"aaaz",											"aaaz" },
		{ NULL,							"",		"a",											"a" },
		{ NULL,							"",		"aa",											"aa" },
		{ NULL,							"",		"aaaa",											"aaaa" },
		{ NULL,							"",		"a+",											"a" },
		{ NULL,							"",		"aa+",											"aa" },

		{ "^a*?\\w",					"",		"z",											"z" },
		{ NULL,							"",		"az",											"a" },
		{ NULL,							"",		"aaaz",											"a" },
		{ NULL,							"",		"a",											"a" },
		{ NULL,							"",		"aa",											"a" },
		{ NULL,							"",		"aaaa",											"a" },
		{ NULL,							"",		"a+",											"a" },
		{ NULL,							"",		"aa+",											"a" },

		{ "^a+\\w",						"",		"az",											"az" },
		{ NULL,							"",		"aaaz",											"aaaz" },
		{ NULL,							"",		"aa",											"aa" },
		{ NULL,							"",		"aaaa",											"aaaa" },
		{ NULL,							"",		"aa+",											"aa" },

		{ "^a?\\w",						"",		"az",											"az" },
		{ NULL,							"",		"aaaz",											"aa" },
		{ NULL,							"",		"aa",											"aa" },
		{ NULL,							"",		"aaaa",											"aa" },
		{ NULL,							"",		"aa+",											"aa" },

		{ "^\\d{8}\\w{2,}",				"",		"1234567890",									"1234567890" },
		{ NULL,							"",		"12345678ab",									"12345678ab" },
		{ NULL,							"",		"12345678__",									"12345678__" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"1234567",										NULL },

		{ "^[aeiou\\d]{4,5}$",			"",		"uoie",											"uoie" },
		{ NULL,							"",		"1234",											"1234" },
		{ NULL,							"",		"12345",										"12345" },
		{ NULL,							"",		"aaaaa",										"aaaaa" },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"123456",										NULL },

		{ "^[aeiou\\d]{4,5}?",			"",		"uoie",											"uoie" },
		{ NULL,							"",		"1234",											"1234" },
		{ NULL,							"",		"12345",										"1234" },
		{ NULL,							"",		"aaaaa",										"aaaa" },
		{ NULL,							"",		"123456",										"1234" },

//		{ "\\A(abc|def)=(\\1){2,3}\\Z",	"",		"abc=abcabc",									"abc=abcabc`abc`abc" },	// \A begin of string, \Z end of string, \1 backref
//		{ NULL,							"",		"def=defdefdef",								"def=defdefdef`def`def" },
//		{ NULL,							"",		"*** Failers",									NULL },
//		{ NULL,							"",		"abc=defdef",									NULL },

//		{ "^(a)(b)(c)(d)(e)(f)(g)(h)(i)(j)(k)\\11*(\\3\\4)\\1(?#)2$",	"",	"",					NULL },
//		{ NULL,							"",		"abcdefghijkcda2",								"abcdefghijkcda2`a`b`c`d`e`f`g`h`i`j`k`cd" },
//		{ NULL,							"",		"abcdefghijkkkkcda2",							"abcdefghijkkkkcda2`a`b`c`d`e`f`g`h`i`j`k`cd" },

		{ "(cat(a(ract|tonic)|erpillar)) \\1()2(3)", "", "",									NULL },
//		{ NULL,							"",		"cataract cataract23",							"cataract cataract23`cataract`aract`ract``3" },
//		{ NULL,							"",		"catatonic catatonic23",						"catatonic catatonic23`catatonic`atonic`tonic``3" },
//		{ NULL,							"",		"caterpillar caterpillar23",					"caterpillar caterpillar23`caterpillar`erpillar```3" },

		{ "^From +([^ ]+) +[a-zA-Z][a-zA-Z][a-zA-Z] +[a-zA-Z][a-zA-Z][a-zA-Z] +[0-9]?[0-9] +[0-9][0-9]:[0-9][0-9]",	"",	"",	NULL },
		{ NULL,							"",		"From abcd  Mon Sep 01 12:33:02 1997",			"From abcd  Mon Sep 01 12:33`abcd" },

		{ "^From\\s+\\S+\\s+([a-zA-Z]{3}\\s+){2}\\d{1,2}\\s+\\d\\d:\\d\\d",	"",	"",				NULL },
		{ NULL,							"",		"From abcd  Mon Sep 01 12:33:02 1997",			"From abcd  Mon Sep 01 12:33`Sep " },
		{ NULL,							"",		"From abcd  Mon Sep  1 12:33:02 1997",			"From abcd  Mon Sep  1 12:33`Sep  " },
		{ NULL,							"",		"*** Failers",									NULL },
		{ NULL,							"",		"From abcd  Sep 01 12:33:02 1997",				NULL },

//		{ "^12.34",						"s",	"12\n34",										"12\\x0a34" },	// PCRE_DOTALL mode
//		{ NULL,							"s",	"12\r34",										"12\\x0d34" },

		{ "^[ab]{1,3}(ab*|b)",			"",		"aabbbbb",										"aabb`b" },
		{ "^[ab]{1,3}?(ab*|b)",			"",		"aabbbbb",										"aabbbbb`abbbbb" },
		{ "^[ab]{1,3}?(ab*?|b)",		"",		"aabbbbb",										"aa`a" },
		{ "^[ab]{1,3}(ab*?|b)",			"",		"aabbbbb",										"aabb`b" },

		// My own test cases:
		{ "^http://([^/]+)(.*)",		"",		"http://localhost",								"http://localhost`localhost`" },
		{ NULL,							"",		"http://localhost/",							"http://localhost/`localhost`/" },
		{ NULL,							"",		"http://localhost/index.html",					"http://localhost/index.html`localhost`/index.html" },
		{ NULL,							"",		"ftp://localhost",								NULL },

		{ "a?a?aa",						"",		"aa",											"aa" },

		{ "",							"",		"",												"" },
		{ "()",							"",		"",												"`" },
		{ "a1?2*",						"",		"a",											"a" },

		{ "[a-c0-9]",					"",		"9",											"9" },
		{ "[^1]",						"",		"abc",											"a" },
		{ "a|0+a",						"",		"0",											NULL },
		{ NULL,							"",		"a",											"a" },
		{ NULL,							"",		"0a",											"0a" },
		{ NULL,							"",		"00a",											"00a" },
		{ "(012|abc|lung)+",			"",		"abclung",										"abclung`lung" },
		{ "abc|def",					"i",	"Abc",											"Abc" },
		{ "abc|ha|def",					"",		"def",											"def" },
		{ "abc|def",					"",		"abc",											"abc" },
		{ "ab*",						"",		"ababc",										"ab" },

		{ "\\bis\\b",					"",		"This island is beautiful",						"is" },

		// Failed case regression
		{ "(a(b))?",					"",		"ab",											"ab`ab`b" },
		{ "a|(b)",						"",		"a",											"a`" },
		{ NULL,							"",		"b",											"b`b" },
		{ "(a|(b))",					"",		"a",											"a`a`" },
		{ NULL,							"",		"b",											"b`b`b" },
		{ "(([a-z]+|([a-z]+[-]+[a-z]+))[.])+",	"",	"local-host.com",							"local-host.`local-host.`local-host`local-host" },

		{ "(([a-z]|(a))[.])",			"",		"a.c",											"a.`a.`a`" },
		{ "(([a-z]|(ab))[.])",			"",		"ab.c",											"ab.`ab.`ab`ab" },
		{ "((c|(a))[.])",				"",		"a.c",											"a.`a.`a`a" },
		{ "((c|(ab))[.])+",				"",		"ab.c",											"ab.`ab.`ab`ab" },
	};

	Regex regex;
	regex.logLevel = 0;
	const roUtf8* regStr = NULL;

	// Match without compilation
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
				for(roSize j=0; j<regex.result.size(); ++j) {
					str += regex.result[j];
					if(j != regex.result.size() - 1)
						str += "`";
				}
				CHECK_EQUAL(result, str.c_str());
			}
		}
		else
			CHECK(regex.result.isEmpty());
	}

	// Match with compilation
	for(roSize i=0; i<roCountof(testData); ++i) {
		if(testData[i][0])
			regStr = testData[i][0];

		Regex::Compiled compiled;
		CHECK(regex.compile(regStr, compiled));
		regex.match(testData[i][2], compiled, testData[i][1]);
		regex.match(testData[i][2], compiled, testData[i][1]);	// Should able to compile once, match multiple times

		const roUtf8* result = testData[i][3];
		if(result) {
			if(regex.result.isEmpty()) {
				CHECK_EQUAL("", regStr);
			}
			if(!regex.result.isEmpty()) {
				String str;
				for(roSize j=0; j<regex.result.size(); ++j) {
					str += regex.result[j];
					if(j != regex.result.size() - 1)
						str += "`";
				}
				CHECK_EQUAL(result, str.c_str());
			}
		}
		else
			CHECK(regex.result.isEmpty());
	}

	regex.match("http://yahoo.com", "http://{:i}\\.{com|org}(\\::z)?{(/:i)*}");
}

TEST_FIXTURE(RegexTest, customMatcher)
{
	struct LocalScope {
		static bool matchFloatNumber(RangedString& inout, void* userData)
		{
			Regex regex;
			if(!regex.match(inout, "([0-9]+\\.[0-9]*)|([0-9]+)|(\\.[0-9]+)")) return false;
			inout.begin = regex.result[0].end;
			return true;
		}

		static bool matchFloatExponent(RangedString& inout, void* userData)
		{
			Regex regex;
			if(!regex.match(inout, "[eE][+-]?[0-9]+")) return false;
			inout.begin = regex.result[0].end;
			return true;
		}
	};	// LocalScope

	Regex regex;

	Array<Regex::CustomMatcher> matchers;
	Regex::CustomMatcher matcher1 = { LocalScope::matchFloatNumber, NULL };
	Regex::CustomMatcher matcher2 = { LocalScope::matchFloatExponent, NULL };
	matchers.pushBack(matcher1);
	matchers.pushBack(matcher2);

	CHECK(regex.match(".23", "$0$1?", matchers));
	CHECK(regex.match("1.23", "$0$1?", matchers));
	CHECK(regex.match("1.23e+01", "$0$1?", matchers));
	CHECK(regex.match("1.23E-01", "$0$1?", matchers));

	CHECK(!regex.match("1.23", "$0$1", matchers));
}
