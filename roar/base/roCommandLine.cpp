#include "pch.h"
#include "roCommandLine.h"
#include "roIOStream.h"
#include "roJson.h"
#include "roLexer.h"
#include "roStringFormat.h"

using namespace ro;

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

struct Option
{
	Option() { flag = Flag_NotSet; }
	static bool match(const Option& lhs, const Option& rhs);
	RangedString preferLongName(const IArray<Option>& optionDesc);

	RangedString shortName;
	RangedString longName;
	RangedString argument;
	RangedString defaultValue;

	enum Flag {
		Flag_NotSet = -1,
		Flag_False = 0,
		Flag_True = 1,
		Flag_NullArg
	};
	Flag flag;
};

bool Option::match(const Option& lhs, const Option& rhs)
{
	if(
		(lhs.shortName.isEmpty() || lhs.shortName != rhs.shortName) &&
		(lhs.longName.isEmpty() || lhs.longName != rhs.longName)
	)
		return false;

	return lhs.argument.isEmpty() == rhs.argument.isEmpty();
}

RangedString Option::preferLongName(const IArray<Option>& optionDesc)
{
	if(!longName.isEmpty()) return longName;
	for(const Option& i : optionDesc) {
		if(shortName == i.shortName)
			return i.longName.isEmpty() ? i.shortName : i.longName;
	}
	return shortName;
}

struct Pattern
{
	enum RegexResultType {
		RegexResultType_None,
		RegexResultType_Argument,
		RegexResultType_Option,
		RegexResultType_GlobalOption
	};
	struct RegexResult {
		RegexResultType type;
		roSize resultIndex;
		RangedString name, value;
		Option option;
	};

	String regexStr;
	Array<RangedString> slot;
	Array<RegexResult> regexResult;
	Array<Option> patternSpecificOptions;
};	// Pattern

struct OptionParser
{
	bool parseOptWithArg(TokenStream& tks, Option& option)
	{
		TokenStream::StateScoped stateScope(tks);
		tks.consumeToken("\n");

		Lexer::Token t;
		if(!tks.currentToken(t))
			return false;

		if(t.token == "shortOption") {
			if(!option.shortName.isEmpty())
				return false;

			Regex regex;
			if(!regex.match(t.value, shortOptionRegStr)) {
				roAssert(false);
				return false;
			}

			if(option.shortName == regex.result[1])	// Check for duplicated name
				return false;

			option.shortName = regex.result[1];

			if(regex.result[2].size())
				option.argument = regex.result[2];
			if(regex.result[3].size())
				option.argument = regex.result[3];

			tks.consumeToken();
			return stateScope.consume(), true;
		}
		else if(t.token == "longOption") {
			if(!option.longName.isEmpty())
				return false;

			Regex regex;
			if(!regex.match(t.value, longOptionRegStr)) {
				roAssert(false);
				return false;
			}

			if(option.longName == regex.result[1])	// Check for duplicated name
				return false;

			option.longName = regex.result[1];

			RangedString argument = regex.result[2].size() ? regex.result[2] : regex.result[3];
			if(argument.size())
				option.argument = argument;

			tks.consumeToken();
			return stateScope.consume(), true;
		}
		else
			return false;
	}

	// optDesc -> optWithArg (optWithArg | (, optWithArg))? comment? default?
	bool parseOptDesc(TokenStream& tks, Option& option)
	{
		if(!parseOptWithArg(tks, option))
			return false;

		Lexer::Token t;
		if(!parseOptWithArg(tks, option)) {
			if(!tks.currentToken(t))
				return true;

			if(t.token == ",") {
				tks.consumeToken();
				if(!parseOptWithArg(tks, option))
					return false;
			}
		}

		if(!tks.currentToken(t))
			return true;

		if(t.token == "comment") {
			tks.consumeToken();
			if(!tks.currentToken(t))
				return true;
		}

		if(t.token == "default") {
			Regex regex;
			if(!regex.match(t.value, defaultValueRegStr, "i"))
				return false;

			option.defaultValue = regex.result[1];
			tks.consumeToken();
		}

		return true;
	}

	// optDescs -> (\s)* optDesc | optDesc \n optDescs
	bool parseOptDescs(TokenStream& tks, IArray<Option>& options)
	{
		Option option;

		Lexer::Token t;

		// optDescs -> optDesc (\n optDescs)?
		if(!parseOptDesc(tks, option))
			return false;

		if(!options.pushBack(option))
			return false;

		if(!tks.currentToken(t))
			return true;

		if(t.token != "\n")
			return false;

		return parseOptDescs(tks, options);
	}

	static bool parseOptDescs(const RangedString& option, IArray<Option>& options)
	{
		if(option.isEmpty())
			return true;

		roStatus st;
		TokenStream tks;
		Lexer& lexer = tks._lexer;

		lexer.registerRegexRule("comment",		"[ \\t]{2,}[^][()<>|\\-\\n]+");
		lexer.registerRegexRule("shortOption",	OptionParser::shortOptionRegStr);
		lexer.registerRegexRule("longOption",	OptionParser::longOptionRegStr);
		lexer.registerRegexRule("default",		"\\[default:\\s*([^][()<>|]+)\\]", "i");
		lexer.registerRegexRule(" ",			"[ \\t]+");
		lexer.registerRegexRule("\n",			"[\\n]+");
		lexer.registerStrRule(",");
		lexer.registerStrRule("=");

		tks.tokenToIgnore.pushBack(" ");
//		tks.tokenToIgnore.pushBack("\n");

		st = tks.beginParse(option);
		if(!st) return st;

		OptionParser optionParser;
		return optionParser.parseOptDescs(tks, options);
	}

	static const char* shortOptionRegStr;
	static const char* longOptionRegStr;
	static const char* argumentRegStr;
	static const char* defaultValueRegStr;
};	// OptionParser

//												   -optionName				argument		  |		<argument>
const char* OptionParser::shortOptionRegStr		= "-([^][()<>|,=\\-\\s])(?: (?:([^][()<>|,=\\-\\s]+)|(?:<([^][()<>|,\\-\\s]+)>)))?";
const char* OptionParser::longOptionRegStr		= "--([^][()<>|,=\\-\\s]+)(?:(?: |=)(?:([^][()<>|,=\\-\\sa-z]+)|<([^][()<>|,=\\-\\s]+)>))?";
const char* OptionParser::argumentRegStr		= " ((?:[^][()<>|\\-\\s]+)|(?:<[^][()<>|\\-\\s]+>))";
const char* OptionParser::defaultValueRegStr	= "\\s*\\[\\s*default\\s*:\\s*([^]]+)\\]";

static bool parseUsage(const RangedString& usage, IArray<Pattern>& patterns, IArray<Option>& optionsDesc)
{
	roStatus st;
	Lexer lexer;

	lexer.registerRegexRule("string",		"[^][()<>|\\s]+");
//	lexer.registerRegexRule("option",		"--?[^][()<>|\\s]+");
	lexer.registerRegexRule("shortOption",	OptionParser::shortOptionRegStr);
	lexer.registerRegexRule("longOption",	OptionParser::longOptionRegStr);
	lexer.registerRegexRule("argument",		"<[^][()<>|]+>");
	lexer.registerRegexRule("options",		"options", "i");
	lexer.registerRegexRule(" ",			"[ \\t]+");
	lexer.registerRegexRule("\n",			"[\\n]+");
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
	Lexer::Token t;

	while(true) {
		st = lexer.nextToken(t);
		if(!st) break;

		if(t.token == " ") continue;

		if(t.token == "\n") {
			patterns.pushBack();
			patterns.back().regexStr = "^\\s*";
			continue;
		}

		Pattern& pattern = patterns.back();
		String& str = pattern.regexStr;
		bool putSpace = !pattern.slot.isEmpty() && str.back() != '|';

		if(t.token == "string") {
			Pattern::RegexResult regexResult = {
				Pattern::RegexResultType_None,
				pattern.regexResult.size() + 1,
				t.value
			};
			pattern.regexResult.pushBack(regexResult);

			pattern.slot.pushBack(t.value);
			if(!uniqueElements.find(t.value))
				uniqueElements.pushBack(t.value);
			strFormat(str, "{}({})", putSpace ? "\\s+" : "", t.value);
		}
		else if(t.token == "argument") {
			Pattern::RegexResult regexResult = {
				Pattern::RegexResultType_Argument,
				pattern.regexResult.size() + 1,
				RangedString(t.value.begin+1, t.value.end-1)	// Remove '<' and '>'
			};
			pattern.regexResult.pushBack(regexResult);

			pattern.slot.pushBack(RangedString(t.value.begin+1, t.value.end-1));	// Remove '<' and '>'
			if(!uniqueElements.find(t.value))
				uniqueElements.pushBack(t.value);
			strFormat(str, "{}($1)", putSpace ? "\\s+" : "");
		}
		else if(t.token == "shortOption") {
			Regex regex;
			if(!regex.match(t.value, OptionParser::shortOptionRegStr)) {
				roAssert(false);
				return false;
			}

			Option option;
			option.shortName = regex.result[1];

			if(regex.result[2].size())
				option.argument = regex.result[2];
			if(regex.result[3].size())
				option.argument = regex.result[3];

			if(regex.result[2].size())
				option.argument = regex.result[2];

			strFormat(str, " *(${})", 2 + pattern.patternSpecificOptions.size());
			pattern.patternSpecificOptions.pushBack(option);

			Pattern::RegexResult regexResult = {
				Pattern::RegexResultType_Option,
				pattern.regexResult.size() + 1,
				option.shortName, "",
				option
			};
			pattern.regexResult.pushBack(regexResult);
		}
		else if(t.token == "longOption") {
			Regex regex;
			if(!regex.match(t.value, OptionParser::longOptionRegStr)) {
				roAssert(false);
				return false;
			}

			Option option;
			option.longName = regex.result[1];

			RangedString argument = regex.result[2].size() ? regex.result[2] : regex.result[3];
			if(argument.size())
				option.argument = argument;

			strFormat(str, " *(${})", 2 + pattern.patternSpecificOptions.size());
			pattern.patternSpecificOptions.pushBack(option);

			Pattern::RegexResult regexResult = {
				Pattern::RegexResultType_Option,
				pattern.regexResult.size() + 1,
				option.longName, "",
				option
			};
			pattern.regexResult.pushBack(regexResult);
		}
		else if(t.token == "options") {
			str.append("($2)");

			Pattern::RegexResult regexResult = {
				Pattern::RegexResultType_GlobalOption,
				pattern.regexResult.size() + 1,
			};
			pattern.regexResult.pushBack(regexResult);
		}
		else if(t.token == "|") {
			str.append("|");
		}
		else if(t.token == "(") {
			str.append("(?:");
		}
		else if(t.token == ")") {
			str.append(")");	// TODO: Ensure () match
		}
		else if(t.token == "[") {
			str.append("(?:");
		}
		else if(t.token == "]") {
			str.append(")?");	// TODO: Ensure [] match
		}
	}

	lexer.endParse();

	for(Pattern& i : patterns)
		i.regexStr.append("\\s*");

	return true;
}

static bool parseOptionInCmd(const roUtf8* cmd, String& outCmd, IArray<Option>& outOptions, const IArray<Option>& optDescription)
{
	if(roStrLen(cmd) == 0) {
		outCmd = cmd;
		outOptions.clear();
		return true;
	}

	roStatus st;
	Lexer lexer;

	const char* shortOptionReg = "(?:^-|\\s+-)([a-zA-Z])(?: ?([^\\s\\-]+))?";
	const char* longOptionReg  = "(?:^--|\\s+--)([a-zA-Z]+)(?:(=|\\s+)([^-\\s\\-]+))?";
	
	lexer.registerRegexRule(" ",			"\\s+");
	lexer.registerRegexRule("other",		"[^-\\s]+");
	lexer.registerRegexRule("shortOption",	shortOptionReg);
	lexer.registerRegexRule("longOption",	longOptionReg);

	st = lexer.beginParse(cmd);
	if(!st) return st;

	Lexer::LineInfo lInfo;
	Lexer::Token t;

	while(true) {
		st = lexer.nextToken(t);
		if(!st) break;

		if(t.token == "shortOption") {
			Option option;
			Regex regex;
			if(!regex.match(t.value, shortOptionReg)) {
				roAssert(false);
				return false;
			}

			option.shortName = regex.result[1];
			option.argument = regex.result[2];
			option.flag = Option::Flag_True;
			outOptions.pushBack(option);
		}
		else if(t.token == "longOption") {
			Option option;
			Regex regex;
			if(!regex.match(t.value, longOptionReg)) {
				roAssert(false);
				return false;
			}

			option.longName = regex.result[1];
			option.argument = regex.result[3];
			option.flag = Option::Flag_True;
			outOptions.pushBack(option);
		}
		else
			outCmd += t.value;
	}

	lexer.endParse();

	// Gather all un-specified shot option in optDescription
/*	for(Option& i : optDescription) {
		bool found = false;
		for(auto& j : outOptions) {
			if(i.shortName == j.shortName) {
				found = true;
				break;
			}
		}
		if(!found)
			outOptions.pushBack(i);
	}*/

	return true;
}

struct RegCustomMatchers
{
	struct UserData {
		Option* optionInPattern;
		IArray<Option>* optionsInCmd;
	};

	struct AllOptionUserData {
		IArray<Option>* optionsDesc;
		IArray<Option>* optionsInCmd;
	};

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

	static bool matchAllOptions(RangedString& inout, void* userData)
	{
		return true;
		AllOptionUserData* u = reinterpret_cast<AllOptionUserData*>(userData);
		IArray<Option>& option = *u->optionsInCmd;
		IArray<Option>& optionDesc = *u->optionsDesc;
		for(Option& i : optionDesc) {
			for(Option& j : option) {
				if(Option::match(i, j)) {
					option.remove(j);
					break;
				}
			}
		}
		return true;
	}

	// Look for any matched options in the command
	static bool matchOption(RangedString& inout, void* userData)
	{
		UserData* u = reinterpret_cast<UserData*>(userData);
		for(Option& i : *u->optionsInCmd) {
			if(Option::match(*u->optionInPattern, i))
				return true;
		}
		return false;
	}
};	// RegCustomMatchers

roStatus roParseCommandLine(const roUtf8* usage, const roUtf8* cmd, ro::String& outJson)
{
	Array<RangedString> usages;
	RangedString usageSection = parseSection("Usage:", usage);
	RangedString optionSection = parseSection("Options:", usage);

	Array<Option> optionsDesc;
	if(!OptionParser::parseOptDescs(optionSection, optionsDesc))
		return roStatus::cmd_line_parse_error;

	Array<Pattern> patterns;
	parseUsage(usageSection, patterns, optionsDesc);

	roStatus st = roStatus::cmd_line_parse_error;

	// First pass: extract options in cmd
	Array<Option> options;
	String cmdWithoutOption;
	if(!parseOptionInCmd(cmd, cmdWithoutOption, options, optionsDesc))
		return st;

	// Second pass: match command, argument and options
	Regex::CustomMatcher _matchers[] = {
		{ RegCustomMatchers::matchEllipsis, NULL },
		{ RegCustomMatchers::matchString, NULL },
	};

	Array<Option> optionsMatched;
	Array<Option>optionsYetToMatch = options;

	Pattern* matchedPattern = NULL;
	for(Pattern& pattern : patterns)
	{
		optionsMatched.clear();
		optionsYetToMatch = options;

		// Deal with pattern specific options
		Array<RegCustomMatchers::UserData> userData;
		for(Option& i : pattern.patternSpecificOptions) {
			RegCustomMatchers::UserData u = { &i, &options };
			userData.pushBack(u);
		}

		Array<Regex::CustomMatcher> matchers;
		for(Regex::CustomMatcher& i : _matchers)
			matchers.pushBack(i);
		for(RegCustomMatchers::UserData& i : userData) {
			Regex::CustomMatcher m = { RegCustomMatchers::matchOption, &i };
			matchers.pushBack(m);
		}

		// Deal with all option specified in "Option:" section
		RegCustomMatchers::AllOptionUserData allOptUserData = { &optionsDesc, &options };
		{	Regex::CustomMatcher m = { RegCustomMatchers::matchAllOptions, &allOptUserData };
			matchers.pushBack(m);
		}

		// Run the regex
		Regex regex;
		if(!regex.match(cmdWithoutOption.c_str(), pattern.regexStr, matchers))
			continue;

		// Parse the match result
		for(Pattern::RegexResult& result : pattern.regexResult) {
			if(!regex.isMatch(result.resultIndex))
				continue;

			if(result.type == Pattern::RegexResultType_Argument)
				result.value = regex.result[result.resultIndex];
			else if(result.type == Pattern::RegexResultType_Option) {
				Option* opt = optionsYetToMatch.find(result.option, Option::match);
				roAssert(opt);
				if(!opt) continue;

				optionsMatched.pushBack(*opt);
				optionsYetToMatch.removeAllByKey(result.option, Option::match);
			}
			else if(result.type == Pattern::RegexResultType_GlobalOption) {

				// Merge rule specific options into global options
				for(Option& i : optionsYetToMatch) {
					Option* opt = optionsDesc.find(i, Option::match);
					if(opt)
						optionsMatched.pushBack(i);
				}
				optionsYetToMatch.removeAllByKeys(optionsDesc, Option::match);
			}
		}

		// See if all option in cmd have been matched
		if(!optionsYetToMatch.isEmpty())
			continue;

		// Need to match all characters in cmd
		if(*regex.result[0].end != '\0')
			continue;

		// Merge rule specific options into global options
/*		for(Option& i : localOptions) {
			Option* opt = roArrayFind(options.begin(), options.size(), i, Option::match);
			if(!opt)
				options.pushBack(*opt);
		}*/

		matchedPattern = &pattern;

		// TODO: Need to try all for longest match?
		break;
	}

	// All option in cmd matched?
	if(!options.isEmpty() && !optionsYetToMatch.isEmpty())
		return roStatus::cmd_line_parse_error;

	// No rule matched?
	if(!matchedPattern && !patterns.isEmpty())
		return roStatus::cmd_line_parse_error;

	// Begin writing JSON
	JsonWriter writer;
	MemoryOStream os;
	writer.setStream(&os);
	writer.beginDocument();
	writer.beginObject();

	// Fill options with default values
	for(Option& i : optionsDesc) {
		Option* opt = roArrayFind(optionsMatched.begin(), optionsMatched.size(), i, Option::match);
		if(!opt) {
			optionsMatched.pushBack(i);
			opt = &optionsMatched.back();
			if(!opt->defaultValue.isEmpty())
				opt->argument = opt->defaultValue;
			else if(!opt->argument.isEmpty())
				opt->flag = Option::Flag_NullArg;
			else if(opt->defaultValue.isEmpty())
				opt->flag = Option::Flag_False;
			else {
				roAssert(false);
			}
		}
	}

	// Output matched global options
	for(Option& opt : optionsMatched) {
		String name = opt.preferLongName(optionsDesc);
		RangedString value;
		if(opt.flag == Option::Flag_NullArg)
			writer.writeNull(name.c_str());
		else if(!opt.argument.isEmpty())
			writer.write(name.c_str(), opt.argument);
		else {
			writer.write(name.c_str(), opt.flag == Option::Flag_True);
		}
	}

	// Output pattern specific stuff
	if(matchedPattern) for(Pattern::RegexResult& result : matchedPattern->regexResult) {
		if(result.type == Pattern::RegexResultType_Argument) {
			if(result.value.isEmpty())
				writer.writeNull(result.name.toString().c_str());
			else
				writer.write(result.name.toString().c_str(), result.value.toString().c_str());
		}
	}

	writer.endObject();
	writer.endDocument();

	const char* p = (const char*)os.bytePtr();
	outJson = p;

	return roStatus::ok;
}