#include "pch.h"
#include "roRegex.h"
#include "roLog.h"
#include "roTypeOf.h"
#include "roTypeCast.h"

// Reference:
// Online regex test platform
// http://regexpal.com/
// Regular Expression Matching Can Be Simple And Fast
// http://swtch.com/~rsc/regexp/regexp1.html
// Regular expression to NFA visualization
// http://hackingoff.com/compilers/regular-expression-to-nfa-dfa	
// A JavaScript and regular expression centric blog
// http://blog.stevenlevithan.com
// Perl regular expression cheat sheet
// http://ult-tex.net/info/perl/
// .Net regex quick reference
// http://msdn.microsoft.com/en-us/library/az24scfc%28v=vs.110%29.aspx

namespace ro {

namespace {

static const char* str_symbols = "*+?|{}()[]^$";
static const char* str_escapes[] = { "nrtvf", "\n\r\t\v\f" };
static const char* str_charClass = "bdDsSwW";
static const char* str_repeatition = "?+*{";
static const char* str_whiteSpace = " \t\r\n\v\f";

static bool charCmp(char c1, char c2) { return c1 == c2; }
static bool charCaseCmp(char c1, char c2) { return roToLower(c1) == roToLower(c2); }

// Will eat '\' on successful escape
char doEscape(const roUtf8*& str)
{
	if(*str != '\\') return *str;


	const char* i = roStrChr(str_escapes[0], str[1]);
	if(i) return ++str, str_escapes[1][i - str_escapes[0]];

	// Make sure we won't touch character class
	i = roStrChr(str_charClass, str[1]);
	if(i) return '\0';

	return *(++str);
}

}	// namespace

struct Node;
struct Graph;

struct Edge
{
	RangedString f;
	typedef bool (*Func)(Graph& graph, Node& node, Edge& edge, RangedString&);
	Func func;
	roUint16 nextNode;
	roUint16 nextEdge;
};	// Edge

struct Node
{
	RangedString debugStr;
	roUint32 edgeCount;
	roUint16 edgeIdx;
	void* userdata[4];
};	// Node

bool loop_repeat(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	// Prevent endless loop, for instance: "(a*)*b" with "b"
	// Where nothing for inner a* to match but it regarded as success since it is '*'
	// then ()* think the inner a* is keeping success, resulting endless loop.
	// Here we try to solve the problem by detecting any progress is made since last loop_repeat
	if(edge.f.begin == s.begin)
		return false;

	edge.f.begin = s.begin;
	return true;
}

bool loop_exit(Graph& graph, Node& node, Edge& edge, RangedString& s) { return true; }

bool counted_loop_repeat(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	roSize& count = (roSize&)node.userdata[0];
	roSize max = (roSize)node.userdata[2];

	if((count++) < max)
		return loop_repeat(graph, node, edge, s);
	else
		return false;
}

bool counted_loop_exit(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	roSize& count = (roSize&)node.userdata[0];
	roSize min = (roSize)node.userdata[1];

	if(count < min)
		return count = 0, false;
	else
		return count = 0, true;
}

bool node_begin(Graph& graph, Node& node, Edge& edge, RangedString& s);
bool node_end(Graph& graph, Node& node, Edge& edge, RangedString& s);
bool pass_though(Graph& graph, Node& node, Edge& edge, RangedString& s) { return true; }
bool alternation(Graph& graph, Node& node, Edge& edge, RangedString& s) { return true; }

bool parse_nodes(Graph& graph, const RangedString& f);

struct Graph
{
	Graph()
	{
		clear();
	}

	void clear()
	{
		regex = NULL;
		customMatchers = NULL;
		nestedGroupLevel = 0;
		branchLevel = 0;
		charCmpFunc = NULL;
		capturingGroupCount = 0;
		tmpResult.clear();
		result.clear();
		nodes2.clear();
		edges.clear();
		currentNodeIdx = endNodeIdx = 0;

		// The first element will not be used
		edges.resize(1);

		// Create the begin node
		Node node = { RangedString("begin") };
		push2(node, node_begin);

	}

	roStatus push2(const Node& node, Edge::Func func, const RangedString& edgeStr=RangedString())
	{
		roStatus st;
		st = roSafeAssign(currentNodeIdx, nodes2.size());
		if(!st) return st;
		st = nodes2.pushBack(node);
		if(!st) return st;
		nodes2.back().edgeCount = 1;
		roUint16 nextNodeIdx;
		st = roSafeAssign(nextNodeIdx, nodes2.size());

		// Create edge for this new node, and point them to the next node
		st = roSafeAssign(nodes2[currentNodeIdx].edgeIdx, edges.size());
		if(!st) return st;

		roUint16 edgeIdx;
		st = roSafeAssign(edgeIdx, edges.size());
		if(!st) return st;

		Edge edge = { edgeStr, func, nextNodeIdx, 0 };
		st = edges.pushBack(edge);
		if(!st) return st;

		return roStatus::ok;
	}

	void pushEdge(roUint16 nodeIdx, const Edge& edge)
	{
		Node& node = nodes2[nodeIdx];
		roUint16 edgeIdx = node.edgeIdx;
		while(true) {
			Edge& edge = edges[edgeIdx];
			if(edge.nextEdge == 0) {
				edge.nextEdge = num_cast<roUint16>(edges.size());
				break;
			}
			edgeIdx = edge.nextEdge;
		}
		edges.pushBack(edge);
		++node.edgeCount;
	}

	Edge& getEdge(roUint16 nodeIdx, roUint16 edgeIdxInNode)
	{
		Node& node = nodes2[nodeIdx];
		roUint16 edgeIdx = node.edgeIdx;
		for(roSize i=0; i<edgeIdxInNode; ++i) {
			roAssert(edgeIdx != 0);
			if(edgeIdx == 0)
				break;
			edgeIdx = edges[edgeIdx].nextEdge;
		}
		return edges[edgeIdx];
	}

	// Redirect all edges that point from fromNode to toNode
	void redirect(roUint16 fromNode, roUint16 toNode)
	{
		for(roSize i=0; i<edges.size(); ++i) {
			Edge& edge = edges[i];
			if(edge.nextNode == fromNode)
				edge.nextNode = toNode;
		}
	}

	Regex* regex;
	const IArray<Regex::CustomMatcher>* customMatchers;
	roSize nestedGroupLevel;
	roSize branchLevel;
	RangedString regString;
	RangedString srcString;
	bool (*charCmpFunc)(char c1, char c2);
	roSize capturingGroupCount;
	TinyArray<RangedString, 16> tmpResult, result;

	TinyArray<Node, 64> nodes2;
	TinyArray<Edge, 128> edges;
	roUint16 currentNodeIdx;
	roUint16 endNodeIdx;
};	// Graph

//////////////////////////////////////////////////////////////////////////
// Matching

bool node_begin(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	// NOTE: This line is to prevent the optimizer to merge node_begin with any other function
	graph.branchLevel = 0;
	return true;
}

bool node_end(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	--graph.branchLevel;
	return true;
}

bool group_begin(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	edge.f.begin = s.begin;
	node.userdata[0] = (void*)s.begin;
	node.userdata[3] = (void*)graph.branchLevel;
	return ++graph.nestedGroupLevel, true;
}
bool group_end(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	roAssert(graph.nestedGroupLevel > 0);
	roUint16 beginNodeIdx = (roUint16)node.userdata[0];
	Node& beginNode = graph.nodes2[beginNodeIdx];

	// Commit the data in beginNode
	// This check prevent altering the begin of string during back-tracking
	// Test case: "(b+|a){1,2}?bc" "bbc"
	if((void*)graph.branchLevel >= beginNode.userdata[3])
		beginNode.userdata[1] = beginNode.userdata[0];
	beginNode.userdata[2] = (void*)s.begin;

	if(beginNode.userdata[1] <= beginNode.userdata[2])
		graph.tmpResult[(roSize)node.userdata[1]] = RangedString((roUtf8*)beginNode.userdata[1], (roUtf8*)beginNode.userdata[2]);

	// Commit the latest successful match at the outermost capturing group
	if(graph.nestedGroupLevel == 1)
		graph.result = graph.tmpResult;

	return --graph.nestedGroupLevel, true;
}

bool nonCaptureGroup_begin(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	return true;
}
bool nonCaptureGroup_end(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	return true;
}

bool match_raw(Graph& graph, Node& node, Edge& edge, RangedString& s_)
{
	const roUtf8* f = edge.f.begin;
	const roUtf8* fend = edge.f.end;
	const roUtf8* s = s_.begin;
	const roUtf8* send = s_.end;

	for(; f < fend && s < send; ++f, ++s) {
		if(*f == '.')
			continue;

		// Escape character
		char escaped = doEscape(f);
		roAssert(escaped != '\0');
		roAssert(s != '\0');

		if(!graph.charCmpFunc(*s, escaped))
			return false;
	}

	if(f == edge.f.begin)
		return false;

	s_.begin = s;
	return true;
}

static bool isWordChar(char c)
{
	return roIsAlpha(c) || roIsDigit(c) || c == '_';
}

bool match_charClass(char f, RangedString& s)
{
	roUtf8 c = *s.begin;
	switch(f) {
	case 'd':
		if(roIsDigit(c)) return true;
		break;
	case 'D':
		if(!roIsDigit(c)) return true;
		break;
	case 's':
		if(roStrChr(str_whiteSpace, c)) return true;
		break;
	case 'S':
		if(!roStrChr(str_whiteSpace, c)) return true;
		break;
	case 'w':
		if(isWordChar(c)) return true;
		break;
	case 'W':
		if(!isWordChar(c)) return true;
		break;
	default:
		break;
	}

	return false;
}

bool match_charClass(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	if(s.begin >= s.end)
		return false;

	if(match_charClass(*edge.f.begin, s))
		return ++s.begin, true;

	return false;
}

static bool isCharSet(const roUtf8* f)
{
	if(f[0] != '\\')
		return false;
	return roStrChr(str_charClass, f[1]) != NULL;
}

bool match_charSet(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	const roUtf8* i = edge.f.begin;

	if(s.begin == s.end)
		return false;

	roUtf8 c = *s.begin;
	bool exclusion= false;
	if(*i == '^')
		exclusion = true, ++i;

	bool inList = false;
	for(; i<edge.f.end; ++i) {
		roUtf8 escaped = doEscape(i);

		// Character range
		if((i + 2) < edge.f.end && i[1] == '-') {
			i += 2;
			roUtf8 escaped2 = doEscape(i);
			roAssert(i < edge.f.end);
			roUtf8 l = roToLower(c);
			roUtf8 u = roToUpper(c);
			if((l >= escaped && l <= escaped2) || (u >= escaped && u <= escaped2)) {
				inList = true;
				break;
			}
		}
		// Character class
		else if((i + 1) < edge.f.end && isCharSet(i)) {
			++i;
			if(match_charClass(*i, s)) {
				inList = true;
				break;
			}
		}
		// Single character
		else {
			if(graph.charCmpFunc(c, escaped)) {
				inList = true;
				break;
			}
		}
	}

	++s.begin;
	return exclusion ? !inList : inList;
}

bool match_beginOfString(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	return (s.begin == graph.srcString.begin);
}

bool match_endOfString(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	return (s.begin == graph.srcString.end);
}

bool match_customFunc(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	const Regex::CustomMatcher* matcher = reinterpret_cast<const Regex::CustomMatcher*>(node.userdata[0]);
	if(!matcher) return false;
	if(!matcher->func) return false;
	return matcher->func(s, matcher->userData);
}

//////////////////////////////////////////////////////////////////////////
// Parsing

bool parse_repeatition(Graph& graph, const RangedString& f, const roUtf8*& i, roUint16 preveNodeIdx=0, roUint16 beginNodeIdx=0)
{
	if(i[0] == '?') {
		Edge edge = { RangedString(), pass_though, graph.currentNodeIdx+1, 0 };
		graph.pushEdge(beginNodeIdx, edge);
	}
	else if(i[0] == '+') {
		bool greedy = (i[1] != '?');
		if(!greedy) ++i;
		roUint16 edge0Idx = greedy ? 0 : 1;
		roUint16 edge1Idx = greedy ? 1 : 0;

		Node node = { RangedString("+") };
		graph.push2(node, Edge::Func(NULL));
		Edge edge = { RangedString(), NULL, 0, 0 };
		graph.pushEdge(graph.currentNodeIdx, edge);
		graph.getEdge(graph.currentNodeIdx, edge0Idx).func = loop_repeat;
		graph.getEdge(graph.currentNodeIdx, edge0Idx).nextNode = beginNodeIdx;
		graph.getEdge(graph.currentNodeIdx, edge1Idx).func = loop_exit;
		graph.getEdge(graph.currentNodeIdx, edge1Idx).nextNode = graph.currentNodeIdx+1;
	}
	else if(i[0] == '*') {
		bool greedy = (i[1] != '?');
		if(!greedy) ++i;
		roUint16 edge0Idx = greedy ? 1 : 0;
		roUint16 edge1Idx = greedy ? 0 : 1;

		Node node = { RangedString("*") };
		graph.push2(node, Edge::Func(NULL));
		Edge edge = { RangedString(), NULL, 0, 0 };
		graph.pushEdge(graph.currentNodeIdx, edge);
		graph.getEdge(graph.currentNodeIdx, edge0Idx).func = loop_exit;
		graph.getEdge(graph.currentNodeIdx, edge0Idx).nextNode = graph.currentNodeIdx+1;
		graph.redirect(beginNodeIdx, graph.currentNodeIdx);
		graph.getEdge(graph.currentNodeIdx, edge1Idx).func = loop_repeat;
		graph.getEdge(graph.currentNodeIdx, edge1Idx).nextNode = beginNodeIdx;
	}
	else if(i[0] == '{') {
		const roUtf8* iBegin = i++;
		const roUtf8* pMin = i;
		const roUtf8* pMax = NULL;
		roSize min = 0, max = 0;

		for(; roIsDigit(*i); ++i);
		if(i == pMin)	// No number after '{'
			return false;

		if(*i == ',') {
			pMax = ++i;
			for(; roIsDigit(*i); ++i);
		}

		if(*i != '}')
			return false;

		const roUtf8* pClose = i;

		bool greedy = (i[1] != '?');
		if(!greedy) ++i;
		roUint16 edge0Idx = greedy ? 1 : 0;
		roUint16 edge1Idx = greedy ? 0 : 1;

		roVerify(sscanf(pMin, "%u", &min) == 1);

		// No max was given, use the biggest number instead
		if(pMax == pClose)
			max = ro::TypeOf<roSize>::valueMax();
		else if(pMax == NULL)
			max = min;
		else
			roVerify(sscanf(pMax, "%u", &max) == 1);

		if(min > max)	// Consider as parsing error
			return false;

		Node node = { RangedString(iBegin, i+1) };
		node.userdata[0] = (void*)0;
		node.userdata[1] = (void*)min;
		node.userdata[2] = (void*)max;

		graph.push2(node, Edge::Func(NULL));
		Edge edge = { RangedString(), NULL, 0, 0 };
		graph.pushEdge(graph.currentNodeIdx, edge);
		graph.getEdge(graph.currentNodeIdx, edge0Idx).func = counted_loop_exit;
		graph.getEdge(graph.currentNodeIdx, edge0Idx).nextNode = graph.currentNodeIdx+1;
		graph.redirect(beginNodeIdx, graph.currentNodeIdx);
		graph.getEdge(graph.currentNodeIdx, edge1Idx).func = counted_loop_repeat;
		graph.getEdge(graph.currentNodeIdx, edge1Idx).nextNode = beginNodeIdx;
	}
	else
		return true;

	++i;
	return true;
}

bool parse_raw(Graph& graph, const RangedString& f, const roUtf8*& i)
{
	if(roStrChr(str_symbols, *i))
		return false;

	// Search for end of raw string
	const roUtf8* end2 = i;
	for(; end2 < f.end; ++end2) {
		if(roStrChr(str_symbols, *end2))
			break;
		if(isCharSet(end2))
			break;
		if(*end2 == '\\')	// Go though any escape character
			++end2;
	}

	roAssert(end2 != i);
	const roUtf8* end1 = end2;

	// See if any repetition follow the string, if yes we divide the string into 2 parts
	if(end2 != f.end && roStrChr(str_repeatition, *end2))
		end1 = end2 - 1;

	// Deal with first part
	if(end1 > i) {
		Node node = { RangedString(i, end1) };
		graph.push2(node, match_raw, RangedString(i, end1));
		i = end1;
	}

	// Deal with second part
	if(end1 != end2) {
		Node node = { RangedString(end1, end2) };
		graph.push2(node, match_raw, RangedString(end1, end2));

		i = end2;
		parse_repeatition(graph, f, i, graph.currentNodeIdx-1, graph.currentNodeIdx);
	}

	return true;
}

bool parse_charSet(Graph& graph, const RangedString& f, const roUtf8*& i)
{
	if(*i != '[') return false;
	const roUtf8* begin = ++i;

	// Look for corresponding ']'
	for(; true; ++i) {
		if(i >= f.end) return false;

		// Escape character
		roUtf8 nonEscaped = *i;
		roUtf8 escaped = doEscape(i);

		if(escaped == ']' && nonEscaped == ']' &&
			i[-1] != '[' &&	// []] is valid, treated as [\]]
			i[-1] != '^'	// [^]] is valid, treated as [^\]]
		) {
			++i;
			break;
		}
	}

	const roUtf8* end = i - 1;

	Node node = { RangedString(begin-1, end+1) };
	graph.push2(node, match_charSet, RangedString(begin, end));
	parse_repeatition(graph, f, i, graph.currentNodeIdx-1, graph.currentNodeIdx);

	return true;
}

bool parse_charClass(Graph& graph, const RangedString& f, const roUtf8*& i)
{
	if(*i != '\\') return false;
	if(!roStrChr(str_charClass, i[1]))
		return false;

	Node node = { RangedString(i, i+2) };
	graph.push2(node, match_charClass, RangedString(i+1, i+2));

	i += 2;
	parse_repeatition(graph, f, i, graph.currentNodeIdx-1, graph.currentNodeIdx);

	return true;
}

bool parse_group(Graph& graph, const RangedString& f, const roUtf8*& i)
{
	if(*i != '(') return false;
	const roUtf8* begin = ++i;

	// Check for non-capturing group
	bool capturing = true;
	if(i[0] == '?' && i[1] == ':') {
		i += 2;
		begin = i;
		capturing = false;
	}

	// Look for corresponding ')'
	for(roSize parenthesesCount = 1; true; ++i) {
		char escaped = doEscape(i);
		parenthesesCount += (escaped == '(') ? 1 : 0;
		parenthesesCount -= (escaped == ')') ? 1 : 0;
		if(i >= f.end) return false;
		if(parenthesesCount == 0) break;
	}

	const roUtf8* end = i;
	roAssert(end >= begin);

	roUint16 prevNodeIdx, beginNodeIdx;
	roSize capturingGroupIdx = 0;

	{	// Add starting node
		Node beginNode = { RangedString("(") };
		prevNodeIdx = graph.currentNodeIdx;
		graph.push2(beginNode, capturing ? group_begin : nonCaptureGroup_begin);
		beginNodeIdx = graph.currentNodeIdx;

		if(capturing) {
			capturingGroupIdx = graph.capturingGroupCount;
			++graph.capturingGroupCount;
			graph.tmpResult.resize(graph.capturingGroupCount);
			graph.result.resize(graph.capturingGroupCount);
		}
	}

	if(!parse_nodes(graph, RangedString(begin, end)))
		return false;

	{	// Add end node
		Node endNode = { RangedString(")") };
		endNode.userdata[0] = (void*)beginNodeIdx;
		endNode.userdata[1] = (void*)capturingGroupIdx;
		graph.push2(endNode, capturing ? group_end : nonCaptureGroup_end);
	}

	roAssert(*i == ')');
	parse_repeatition(graph, f, ++i, prevNodeIdx, beginNodeIdx);

	return true;
}

bool parse_match_beginOfString(Graph& graph, const RangedString& f, const roUtf8*& i)
{
	if(*i != '^') return false;
	Node node = { RangedString("^") };
	graph.push2(node, match_beginOfString);
	return ++i, true;
}

bool parse_match_endOfString(Graph& graph, const RangedString& f, const roUtf8*& i)
{
	if(*i != '$') return false;
	Node node = { RangedString("$") };
	graph.push2(node, match_endOfString);
	return ++i, true;
}

bool parse_customFunc(Graph& graph, const RangedString& f, const roUtf8*& i)
{
	if(!graph.customMatchers) return false;
	if(*i != '$') return false;
	if(!f.isInRange(i - f.begin)) return false;

	roUtf8 idx = 0;
	if(!roStrTo(i+1, i, idx)) return false;

	if(!graph.customMatchers->isInRange(idx)) return false;
	Node node = { RangedString("$") };
	node.userdata[0] = (void*)&(*graph.customMatchers)[idx];
	graph.push2(node, match_customFunc);

	parse_repeatition(graph, f, i, graph.currentNodeIdx-1, graph.currentNodeIdx);

	return true;
}

bool parse_nodes_impl(Graph& graph, const RangedString& f)
{
	bool ret = false;
	for(const roUtf8* i = f.begin; i < f.end; )
	{
		bool b;
		ret |= b = parse_charClass(graph, f, i);			if(b) continue;
		ret |= b = parse_raw(graph, f, i);					if(b) continue;
		ret |= b = parse_charSet(graph, f, i);				if(b) continue;
		ret |= b = parse_group(graph, f, i);				if(b) continue;
		ret |= b = parse_customFunc(graph, f, i);			if(b) continue;
		ret |= b = parse_match_beginOfString(graph, f, i);	if(b) continue;
		ret |= b = parse_match_endOfString(graph, f, i);	if(b) continue;
		break;
	}

	return ret;
}

// parse_nodes() will call parse_nodes_impl() and then parse_group which() and in turn call parse_nodes() recursively
bool parse_nodes(Graph& graph, const RangedString& f)
{
	roSize bracketCount1 = 0;
	roSize bracketCount2 = 0;
	const roUtf8* i = f.begin;
	const roUtf8* begin = f.begin;
	TinyArray<roUint16, 16> altOptionIdx;

	roUint16 altNodeIdx = graph.currentNodeIdx;
	bool firstEncounter = true;

	// Scan till next '|' or till end
	for(;i <= f.end; ++i)
	{
		if(*i == '\\') {
			++i;
			continue;
		}

		if( bracketCount1 == 0 && bracketCount2 == 0 &&
			((i[-1] != '\\' && i[0] == '|') || i == f.end)
		)
		{
			// Add alternate node for the first encountered '|', adding this
			// extra node help preventing un-necessary interaction with '(' node
			if(i[0] == '|' && firstEncounter) {
				Node node = { RangedString("|") };
				graph.push2(node, alternation);
				altNodeIdx = graph.currentNodeIdx;
				firstEncounter = false;
			}

			roUint16 tmp = graph.currentNodeIdx + 1;
			if(!parse_nodes_impl(graph, RangedString(begin, i)))
				return false;

			begin = i + 1;
			altOptionIdx.pushBack(tmp);
		}

		bracketCount1 += (*i == '(') ? 1 : 0;
		bracketCount1 -= (*i == ')') ? 1 : 0;
		bracketCount2 += (*i == '[') ? 1 : 0;
		// Due to the complexing of handling ']' in character set, we simply make a check before decrement
		bracketCount2 -= (*i == ']' && bracketCount2) ? 1 : 0;
	}

	// If there were alternation, adjust the edges
	for(roSize j=1; j<altOptionIdx.size(); ++j)
		graph.redirect(altOptionIdx[j], num_cast<roUint16>(graph.nodes2.size()));

	for(roSize j=1; j<altOptionIdx.size(); ++j) {
		Edge edge = { RangedString(), alternation, altOptionIdx[j], 0 };
		graph.pushEdge(altNodeIdx, edge);
	}

	return true;
}

bool matchNodes(Graph& graph, Node& node, RangedString& s)
{
	Node* pNode = &node;

	// Go straight forward if no branching
	while(pNode->edgeCount == 1)
	{
		Edge& edge = graph.edges[pNode->edgeIdx];
		if(edge.func == node_end)
			return node_end(graph, *pNode, edge, s);

		if(graph.regex->logLevel > 1) {
			String ident = " ";
			ident *= graph.branchLevel;
			roLog("debug", "Matching %s%s against %s\n",
				ident.c_str(),
				pNode->debugStr.toString().c_str(),
				s.toString().c_str()
			);
		}

		if(!(*edge.func)(graph, *pNode, edge, s))
			return --graph.branchLevel, false;

		pNode = &graph.nodes2[edge.nextNode];
	}

	// Call recursively if there is branching
	for(roUint16 edgeIdx = pNode->edgeIdx; edgeIdx;) {
		const roUtf8* sBackup = s.begin;
		roSize levelBackup = graph.nestedGroupLevel;
		void* userdataBackup = pNode->userdata[0];

		Edge& edge = graph.edges[edgeIdx];
		if(edge.func == node_end)
			return node_end(graph, *pNode, edge, s);

		if((*edge.func)(graph, *pNode, edge, s)) {
			Node* nextNode = &graph.nodes2[edge.nextNode];

			++graph.branchLevel;
			if(matchNodes(graph, *nextNode, s))
				return --graph.branchLevel, true;
		}

		// Edge fail, restore the string and try another edge
		s.begin = sBackup;
		graph.nestedGroupLevel = levelBackup;
		pNode->userdata[0] = userdataBackup;

		edgeIdx = edge.nextEdge;
	}

	return --graph.branchLevel, false;
}

Regex::Regex()
	: logLevel(0)
{
}

static void debugNodes(Graph& graph)
{
	for(roSize i=0; i<graph.nodes2.size(); ++i) {
		Node& node = graph.nodes2[i];
		roLog("debug", "Node %u: %s\n", i, node.debugStr.toString().c_str());
		roUint16 edgeIdx = node.edgeIdx;
		do {
			Edge& edge = graph.edges[edgeIdx];
			roLog("debug", "  edge -> node %u\n", edge.nextNode);
			edgeIdx = edge.nextEdge;
		} while(edgeIdx);
	}
	roLog("debug", "\n");
}

bool Regex::match(const roUtf8* s, const roUtf8* f, const char* options)
{
	RangedString regString(f, f + roStrLen(f));
	RangedString srcString(s, s + roStrLen(s));

	return match(srcString, regString, options);
}

bool Regex::match(RangedString srcString, RangedString regString, const char* options)
{
	Array<CustomMatcher> customMatcher;
	return match(srcString, regString, customMatcher, options);
}

bool Regex::match(RangedString srcString, RangedString regString, const IArray<CustomMatcher>& customMatcher, const char* options)
{
	Graph graph;
	graph.regex = this;
	graph.customMatchers = &customMatcher;
	graph.regString = regString;
	graph.srcString = srcString;
	graph.charCmpFunc = charCmp;

	if(options && roStrChr(options, 'i')) {
		graph.charCmpFunc = charCaseCmp;
	}

	// Parsing
	result.clear();
	if(!parse_nodes(graph, regString))
		return false;

	// Push end node
	{	Node node = { RangedString("end") };
		graph.push2(node, node_end);
		graph.edges.back().nextNode = 0;
		graph.redirect(graph.currentNodeIdx+1, graph.currentNodeIdx);
	}

	if(logLevel > 0) {
		roLog("debug", "%s, %s\n", regString.toString().c_str(), srcString.toString().c_str());
		debugNodes(graph);
	}

	// Matching
	while(srcString.begin < srcString.end) {
		const roUtf8* currentBegin = srcString.begin;
		graph.branchLevel = 0;
		graph.nestedGroupLevel = 0;

		if(matchNodes(graph, graph.nodes2.front(), srcString)) {
			result.pushBack(RangedString(currentBegin, srcString.begin));
			for(roSize i=0; i<graph.result.size(); ++i)
				result.pushBack(graph.result[i]);

			return true;
		}

		// If no match found, try start at next character in the string
		srcString.begin = currentBegin + 1;
	}

	result.clear();
	return false;
}

Status Regex::getValue(roSize index, RangedString& str)
{
	if(!result.isInRange(index))
		return Status::index_out_of_range;
	str = result[index];
	return Status::ok;
}

}	// namespace ro
