#include "pch.h"
#include "roCommandLine.h"
#include "roIOStream.h"
#include "roJson.h"
#include "roLexer.h"
#include "roStringFormat.h"

using namespace ro;

namespace {



}	// namespace

static RangedString parseSection(const roUtf8* name, const roUtf8* source)
{
	String regStr;
	if(!strFormat(regStr,
		"(?:^|\\n)"								// anchored at a linebreak (or start of string)
		"([^\\n]*"  "{}"  "[^\\n]*(?=\\n?))"	// a line that contains the name
		"(\\n[ \\t]+[^\\n]+)*"					// followed by any number of lines that are indented
		, name
	))
		return "";

	Regex regex;
	if(!regex.match(source, regStr, "i"))
		return "";

	return RangedString(regex.result[1].end, regex.result[0].end);
}

struct RegCustomMatchers
{
	static bool matchEllipsis(RangedString& inout, void* userData)
	{
		return false;
	}
	static bool matchString(RangedString& inout, void* userData)
	{
		Regex regex;
		if(!regex.match(inout, "\\S+")) return false;
		inout.begin = regex.result[0].end;
		return true;
	}
};	// LocalScope

struct Pattern
{
	String regexStr;
	Array<RangedString> slot;
	Array<bool> isArgument;
};

static bool parseUsage(const RangedString& usage, IArray<Pattern>& patterns)
{
	roStatus st;
	Lexer lexer;

	lexer.registerRegexRule("string",	"[^][()<>|\\s]+");
	lexer.registerRegexRule("option",	"--?[^][()<>|\\s]+");
	lexer.registerRegexRule("argument",	"<[^>]+>");
	lexer.registerRegexRule(" ",		"[ \\t]+");
	lexer.registerRegexRule("\n",		"[\\n]+");
	lexer.registerStrRule("...");
	lexer.registerStrRule("[");
	lexer.registerStrRule("]");
	lexer.registerStrRule("(");
	lexer.registerStrRule(")");
	lexer.registerStrRule("<");
	lexer.registerStrRule(">");
	lexer.registerStrRule("|");
	lexer.registerStrRule("=");

	st = lexer.beginParse(usage);
	if(!st) return st;

	Array<RangedString> uniqueElements;

	Lexer::LineInfo lInfo;
	RangedString t, v;

	while(true) {
		st = lexer.nextToken(t, v, lInfo);
		if(!st) break;

		if(t == " ") continue;

		if(t == "\n") {
			patterns.pushBack();
			patterns.back().regexStr = "^\\s*";
			continue;
		}

		Pattern& pattern = patterns.back();
		String& str = pattern.regexStr;
		bool putSpace = !pattern.slot.isEmpty() && str.back() != '|';

		if(t == "string") {
			pattern.slot.pushBack(v);
			pattern.isArgument.pushBack(false);
			if(!uniqueElements.find(v))
				uniqueElements.pushBack(v);
			strFormat(str, "{}({})", putSpace ? "\\s+" : "", v);
		}
		else if(t == "argument") {
			pattern.slot.pushBack(RangedString(v.begin+1, v.end-1));	// Remove '<' and '>'
			pattern.isArgument.pushBack(true);
			if(!uniqueElements.find(v))
				uniqueElements.pushBack(v);
			strFormat(str, "{}($1)", putSpace ? "\\s+" : "");
		}
		else if(t == "|") {
			str.append("|");
		}
		else if(t == "(") {
			str.append("(?:");
		}
		else if(t == ")") {
			str.append(")");	// TODO: Ensure () match
		}
		else if(t == "[") {
			str.append("(?:");
		}
		else if(t == "]") {
			str.append(")?");	// TODO: Ensure [] match
		}
	}

	lexer.endParse();

	for(roSize i=0; i<patterns.size(); ++i)
		patterns[i].regexStr.append("\\s*");

	return true;
}

roStatus roParseCommandLine(const roUtf8* usage, const roUtf8* cmd, ro::String& outJson)
{
	Array<RangedString> usages;
	RangedString usageSection = parseSection("Usage:", usage);
	RangedString optionSection = parseSection("Options:", usage);

	Array<Pattern> patterns;
	parseUsage(usageSection, patterns);

	JsonWriter writer;
	MemoryOStream os;
	writer.setStream(&os);
	writer.beginDocument();
	writer.beginObject();

	Array<Regex::CustomMatcher> matchers;
	Regex::CustomMatcher _matchers[] = {
		{ RegCustomMatchers::matchEllipsis, NULL },
		{ RegCustomMatchers::matchString, NULL }
	};
	for(roSize i=0; i<roCountof(_matchers); ++i)
		matchers.pushBack(_matchers[i]);

	roStatus st = roStatus::cmd_line_parse_error;

	for(roSize i=0; i<patterns.size(); ++i) {
		Regex regex;
		Pattern& pattern = patterns[i];
		if(!regex.match(cmd, pattern.regexStr, matchers))
			continue;

		for(roSize j=1; j<regex.result.size(); ++j) {
			if(pattern.isArgument[j-1]) {
				writer.write(
					String(pattern.slot[j-1]).c_str(), String(regex.result[j]).c_str()
				);
			}
			else {
				writer.write(
					String(pattern.slot[j-1]).c_str(), true
				);
			}
		}

		// Need to match all characters in cmd
		if(*regex.result[0].end != '\0') {
			writer.endObject();
			writer.endDocument();
			os.clear();
			writer.beginDocument();
			writer.beginObject();
			continue;
		}

		// TODO: Need to try all for longest match?
		st = roStatus::ok;
		break;
	}

	writer.endObject();
	writer.endDocument();
	const char* p = (const char*)os.bytePtr();
	outJson = p;

	return st;

	Regex regex;
	regex.match(cmd, "(ship)\\s+($1)\\s+(move)\\s+($1)\\s+($1)", matchers);
	regex.match(cmd, "(ship)\\s+(new)\\s+$0+", matchers);
	regex.match(cmd, "(ship)\\s+($1)\\s+(move)\\s+($1)\\s+($1)\\s+(--speed)=($1)", matchers);
	regex.match(cmd, "(ship)\\s+(shoot)\\s+($1)\\s+($1)", matchers);
	regex.match(cmd, "(mine)\\s+(set|remove)\\s+($1)\\s+($1)\\s+(--moored|--drifting)", matchers);
	regex.match(cmd, "(-h|--help)", matchers);
	regex.match(cmd, "(--version)", matchers);

	return roStatus::ok;
}