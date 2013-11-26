#include "pch.h"
#include "roRegex.h"
#include "roLog.h"
#include "roTypeOf.h"

// Reference:
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

// Will eat '\' on successful scan
char scanMeta(const roUtf8*& str, char c)
{
	if(*str != '\\') return *str == c;
	return ++str, false;
}

}	// namespace

struct Edge;
struct Node;
struct Graph;

struct Edge
{
	RangedString f;
	bool (*func)(Graph& graph, Node& node, Edge& edge, RangedString&);
	roPtrInt nextNode;
};	// Edge

struct Node
{
	roUint32 edgeCount;
	RangedString debugStr;
	void* userdata[4];
	Edge edges;

	roSize sizeInBytes() const
	{
		return sizeof(Node) + (edgeCount - 1) * sizeof(Edge);
	}

	Edge& edge(roSize i)
	{
		roAssert(i < edgeCount);
		return (&edges)[i];
	}

	bool directEdgeToNode(Edge& edge, Node& node)
	{
		bool isValidEdge = isValid(edge);
		roAssert(isValidEdge);
		if(!isValidEdge) return false;
		edge.nextNode = (roByte*)(&node) - (roByte*)(&edge);
		return true;
	}

	bool isValid(Edge& edge)
	{
		for(roSize i=0; i<edgeCount; ++i) {
			if(&edge == (&edges + i))
				return true;
		}
		return false;
	}
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

bool node_begin(Graph& graph, Node& node, Edge& edge, RangedString& s) { return true; }
bool node_end(Graph& graph, Node& node, Edge& edge, RangedString& s);
bool pass_though(Graph& graph, Node& node, Edge& edge, RangedString& s) { return true; }
bool alternation(Graph& graph, Node& node, Edge& edge, RangedString& s) { return true; }

bool parse_nodes(Graph& graph, const RangedString& f);

struct Graph
{
	Graph()
		: regex(NULL)
		, nestedGroupLevel(0)
		, branchLevel(0)
		, charCmpFunc(NULL)
	{
		{	// Create the begin node
			Node node = { 1, RangedString("begin") };
			node.edges.func = node_begin;
			nodes.insert(0, (roByte*)&node, sizeof(node));

			currentNode = nodeFromAbsOffset(0);
		}

		{	// Create the end node
			Node node = { 1, RangedString("end") };
			node.edges.func = node_end;
			node.edges.nextNode = 0;
			nodes.insert(sizeof(node), (roByte*)&node, sizeof(node));
			currentNode = reinterpret_cast<Node*>(nodes.typedPtr());
			endNode = currentNode + 1;

			roVerify(currentNode->directEdgeToNode(currentNode->edges, *endNode));
			roVerify(endNode->directEdgeToNode(endNode->edges, *endNode));
		}
	}

	Node* push(const Node& node, roSize argCount=0, ...)
	{
		roByte* buffBegin = nodes.typedPtr();

		roSize endNodeOffset = absOffsetFromNode(*endNode);
		roSize byteSize = node.sizeInBytes();
		if(!nodes.insert(endNodeOffset, (roByte*)&node, byteSize)) return NULL;
		currentNode = nodeFromAbsOffset(endNodeOffset);

		// If the buffer get re-allocated, try to adjust the pointers
		roByte* newBuffBegin = nodes.typedPtr();
		if(newBuffBegin != buffBegin) {
			roPtrInt byteOffset = newBuffBegin - buffBegin;	// NOTE: Will roPtrInt get out of range?
			va_list vl;
			va_start(vl, argCount);
			for(roSize i=0; i<argCount; ++i)
				*(va_arg(vl, roByte**)) += byteOffset;
			va_end(vl);
		}

		endNode = nodeFromAbsOffset(nodes.sizeInByte() - sizeof(Node));

		// Initialize all edges pointing to a valid node location anyway, to avoid crash
		for(roSize i=0; i<node.edgeCount; ++i) {
			Edge& e = (&currentNode->edges)[i];
			roVerify(currentNode->directEdgeToNode(e, *endNode));
		}

		return currentNode;
	}

	Edge* pushEdge(Node*& node, const Edge& edge, roSize argCount=0, ...)
	{
		roAssert(nodes.isInRange(&node->edges));
		roAssert(node->edgeCount > 0);

		Node* oldNode = node;
		roByte* buffBegin = nodes.typedPtr();

		roSize edgeOffset = (roByte*)(&node->edges) - nodes.typedPtr();
		edgeOffset += node->edgeCount * sizeof(Edge);
		if(!nodes.insert(edgeOffset, (roByte*)&edge, sizeof(Edge))) return false;

		roByte* newBuffBegin = nodes.typedPtr();
		{
			roPtrInt byteOffset = newBuffBegin - buffBegin;	// NOTE: Will roPtrInt get out of range?

			*((roByte**)&node) += byteOffset;
			*((roByte**)&currentNode) += (byteOffset + (currentNode > oldNode ? sizeof(Edge) : 0));
			*((roByte**)&endNode) += (byteOffset + (endNode > oldNode ? sizeof(Edge) : 0));

			va_list vl;
			va_start(vl, argCount);
			for(roSize i=0; i<argCount; ++i) {
				roByte*& p = *va_arg(vl, roByte**);
				p += (byteOffset + (p > (roByte*)oldNode ? sizeof(Edge) : 0));
			}
			va_end(vl);
		}

		// Adjust relative offset of all existing edges
		for(roSize i=0; i<node->edgeCount; ++i) {
			roPtrInt& offset = node->edge(i).nextNode;
			if(offset > 0) offset += sizeof(Edge);
		}

		++node->edgeCount;

		return &node->edge(node->edgeCount - 1);
	}

	Node* followEdge(Edge& edge)
	{
		return reinterpret_cast<Node*>(((roByte*)&edge) + edge.nextNode);
	}

	roSize absOffsetFromNode(Node& node)
	{
		roByte* p = reinterpret_cast<roByte*>(&node);
		roAssert(p >= nodes.typedPtr());
		return p - nodes.typedPtr();
	}

	Node* nodeFromAbsOffset(roSize offset)
	{
		return reinterpret_cast<Node*>(&nodes[offset]);
	}

	bool redirect(Node& from, Node& to)
	{
		// Scan for every node and edge
		roSize offset = 0;
		while(offset < nodes.sizeInByte()) {
			Node* node = nodeFromAbsOffset(offset);
			
			for(roSize i=0; i<node->edgeCount; ++i) {
				Edge& e = (&node->edges)[i];
				if(reinterpret_cast<Node*>(((roByte*)&e) + e.nextNode) == &from) {
					if(!node->directEdgeToNode(e, to))
						return false;
				}
			}

			offset += node->sizeInBytes();
		}

		return true;
	}

	Regex* regex;
	Array<roByte> nodes;
	Node* currentNode;
	Node* endNode;
	roSize nestedGroupLevel;
	roSize branchLevel;
	RangedString regString;
	RangedString srcString;
	bool (*charCmpFunc)(char c1, char c2);
	Array<roSize> resultEndNodeOffset;
};	// Graph

//////////////////////////////////////////////////////////////////////////
// Matching

bool node_end(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	// NOTE: This line is to prevent the optimizer to merge node_end and node_begin
	// together into the same function
	graph.branchLevel = 0;

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
	roSize beginNodeOffset = (roSize)node.userdata[0];
	Node* beginNode = graph.nodeFromAbsOffset(beginNodeOffset);

	// Commit the data in beginNode
	// This check prevent altering the begin of string during back-tracking
	// Test case: "(b+|a){1,2}?bc" "bbc"
	if((void*)graph.branchLevel >= beginNode->userdata[3])
		beginNode->userdata[1] = beginNode->userdata[0];
	beginNode->userdata[2] = (void*)s.begin;

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

//////////////////////////////////////////////////////////////////////////
// Parsing

bool parse_repeatition(Graph& graph, Node* prevNode, Node* beginNode, Node* endNode, const RangedString& f, const roUtf8*& i)
{
	if(i[0] == '?') {
		Edge edge = { RangedString(), pass_though, 0 };
		Edge* newEdge = graph.pushEdge(beginNode, edge, 1, &endNode);
		roVerify(beginNode->directEdgeToNode(*newEdge, *endNode));
	}
	else if(i[0] == '+') {
		bool greedy = (i[1] != '?');
		if(!greedy) ++i;
		roSize edge0Idx = greedy ? 0 : 1;
		roSize edge1Idx = greedy ? 1 : 0;

		Node node = { 2, RangedString("+") };
		Node* loopNode = graph.push(node, 3, &prevNode, &beginNode, &endNode);
		loopNode->edge(edge0Idx).func = loop_repeat;
		roVerify(loopNode->directEdgeToNode(loopNode->edge(edge0Idx), *beginNode));

		loopNode->edge(edge1Idx).func = loop_exit;
	}
	else if(i[0] == '*') {
		bool greedy = (i[1] != '?');
		if(!greedy) ++i;
		roSize edge0Idx = greedy ? 1 : 0;
		roSize edge1Idx = greedy ? 0 : 1;

		Node node = { 2, RangedString("*") };
		Node* loopNode = graph.push(node, 3, &prevNode, &beginNode, &endNode);
		loopNode->edge(edge0Idx).func = loop_exit;
		roVerify(graph.redirect(*beginNode, *loopNode));

		loopNode->edge(edge1Idx).func = loop_repeat;
		roVerify(loopNode->directEdgeToNode(loopNode->edge(edge1Idx), *beginNode));
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
		roSize edge0Idx = greedy ? 1 : 0;
		roSize edge1Idx = greedy ? 0 : 1;

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

		Node node = { 2, RangedString(iBegin, i+1) };
		Node* loopNode = graph.push(node, 3, &prevNode, &beginNode, &endNode);
		loopNode->edge(edge0Idx).func = counted_loop_exit;
		roVerify(graph.redirect(*beginNode, *loopNode));

		loopNode->edge(edge1Idx).func = counted_loop_repeat;
		roVerify(loopNode->directEdgeToNode(loopNode->edge(edge1Idx), *beginNode));

		loopNode->userdata[0] = (void*)0;
		loopNode->userdata[1] = (void*)min;
		loopNode->userdata[2] = (void*)max;
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
		Node node = { 1, RangedString(i, end1) };
		node.edges.f = RangedString(i, end1);
		node.edges.func = match_raw;
		graph.push(node);
		i = end1;
	}

	// Deal with second part
	if(end1 != end2) {
		Node node = { 1, RangedString(end1, end2) };
		node.edges.f = RangedString(end1, end2);
		node.edges.func = match_raw;

		Node* prevNode = graph.currentNode;
		Node* beginNode = graph.push(node, 1, &prevNode);
		Node* endNode = graph.endNode;

		i = end2;
		parse_repeatition(graph, prevNode, beginNode, endNode, f, i);
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

	Node node = { 1, RangedString(begin-1, end+1) };
	node.edges.f = RangedString(begin, end);
	node.edges.func = match_charSet;

	Node* prevNode = graph.currentNode;
	Node* beginNode = graph.push(node, 1, &prevNode);
	Node* endNode = graph.endNode;

	parse_repeatition(graph, prevNode, beginNode, endNode, f, i);

	return true;
}

bool parse_charClass(Graph& graph, const RangedString& f, const roUtf8*& i)
{
	if(*i != '\\') return false;
	if(!roStrChr(str_charClass, i[1]))
		return false;

	Node node = { 1, RangedString(i, i+2) };
	node.edges.f = RangedString(i+1, i+2);
	node.edges.func = match_charClass;

	Node* prevNode = graph.currentNode;
	Node* beginNode = graph.push(node, 1, &prevNode);
	Node* endNode = graph.endNode;

	i += 2;
	parse_repeatition(graph, prevNode, beginNode, endNode, f, i);

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

	roSize prevNodeOffset, beginNodeOffset;

	{	// Add starting node
		Node beginNode = { 1, RangedString("(") };
		beginNode.edges.func = capturing ? group_begin : nonCaptureGroup_begin;
		prevNodeOffset = graph.absOffsetFromNode(*graph.currentNode);
		beginNodeOffset = graph.absOffsetFromNode(*graph.push(beginNode));

		if(capturing)
			graph.resultEndNodeOffset.pushBack(beginNodeOffset);
	}

	if(!parse_nodes(graph, RangedString(begin, end)))
		return false;

	{	// Add end node
		Node endNode = { 1, RangedString(")") };
		endNode.edges.func = capturing ? group_end : nonCaptureGroup_end;
		endNode.userdata[0] = (void*)beginNodeOffset;
		graph.push(endNode);
	}

	Node* prevNode = graph.nodeFromAbsOffset(prevNodeOffset);
	Node* beginNode = graph.nodeFromAbsOffset(beginNodeOffset);

	roAssert(*i == ')');
	parse_repeatition(graph, prevNode, beginNode, graph.endNode, f, ++i);

	return true;
}

bool parse_match_beginOfString(Graph& graph, const RangedString& f, const roUtf8*& i)
{
	if(*i != '^') return false;
	Node node = { 1, RangedString("^") };
	node.edges.func = match_beginOfString;
	graph.push(node);
	return ++i, true;
}

bool parse_match_endOfString(Graph& graph, const RangedString& f, const roUtf8*& i)
{
	if(*i != '$') return false;
	Node node = { 1, RangedString("$") };
	node.edges.func = match_endOfString;
	graph.push(node);
	return ++i, true;
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
		ret |= b = parse_match_beginOfString(graph, f, i);	if(b) continue;
		ret |= b = parse_match_endOfString(graph, f, i);	if(b) continue;
		break;
	}

	return ret;
}

bool parse_nodes(Graph& graph, const RangedString& f)
{
	roSize bracketCount1 = 0;
	roSize bracketCount2 = 0;
	const roUtf8* i = f.begin;
	const roUtf8* begin = f.begin;
	TinyArray<roSize, 16> absOffsets;

	roSize beginNodeOffset = graph.absOffsetFromNode(*graph.currentNode);
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
				Node node = { 1, RangedString("|") };
				node.edges.func = alternation;
				graph.push(node);
				beginNodeOffset = graph.absOffsetFromNode(*graph.currentNode);
				firstEncounter = false;
			}

			if(!parse_nodes_impl(graph, RangedString(begin, i)))
				return false;

			begin = i + 1;
			absOffsets.pushBack(graph.absOffsetFromNode(*graph.currentNode));
		}

		bracketCount1 += (*i == '(') ? 1 : 0;
		bracketCount1 -= (*i == ')') ? 1 : 0;
		bracketCount2 += (*i == '[') ? 1 : 0;
		// Due to the complexing of handling ']' in character set, we simply make a check before decrement
		bracketCount2 -= (*i == ']' && bracketCount2) ? 1 : 0;
	}

	// If there were alternation, adjust the edges
	for(roSize j=1; j<absOffsets.size(); ++j) {
//		Node* node1 = graph.nodeFromAbsOffset(absOffsets[j-1]);
		Node* node2 = graph.nodeFromAbsOffset(absOffsets[j-0]);
		roVerify(graph.redirect(*node2, *graph.endNode));
	}

	Node* beginNode = graph.nodeFromAbsOffset(beginNodeOffset);
	for(roSize j=1; j<absOffsets.size(); ++j) {
		Edge edge = { RangedString(), alternation, 0 };
		Edge* newEdge = graph.pushEdge(beginNode, edge);
		Node* node = graph.nodeFromAbsOffset(absOffsets[j] + j * sizeof(Edge));
		roVerify(beginNode->directEdgeToNode(*newEdge, *node));
	}

	return true;
}

bool matchNodes(Graph& graph, Node& node, RangedString& s)
{
	Node* pNode = &node;

	// Go straight forward if no branching
	while(pNode->edgeCount == 1)
	{
		Edge& edge = pNode->edges;
		if(edge.func == node_end)
			return --graph.branchLevel, true;

		if(graph.regex->isDebug) {
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

		pNode = graph.followEdge(edge);
	}

	// Call recursively if there is branching
	for(roSize i=0; i<pNode->edgeCount; ++i) {
		const roUtf8* sBackup = s.begin;
		roSize levelBackup = graph.nestedGroupLevel;
		void* userdataBackup = pNode->userdata[0];

		Edge& edge = (&pNode->edges)[i];
		if(edge.func == node_end)
			return --graph.branchLevel, true;

		if((*edge.func)(graph, *pNode, edge, s)) {
			Node* nextNode = graph.followEdge(edge);
			if(!nextNode)
				continue;

			++graph.branchLevel;
			if(matchNodes(graph, *nextNode, s))
				return --graph.branchLevel, true;
		}

		// Edge fail, restore the string and try another edge
		s.begin = sBackup;
		graph.nestedGroupLevel = levelBackup;
		pNode->userdata[0] = userdataBackup;
	}

	return --graph.branchLevel, false;
}

Regex::Regex()
	: isDebug(false)
{
}

static void debugNodes(Graph& graph)
{
	TinyArray<Node*, 64> nodePtrs;
	roSize offset = 0;

	while(offset < graph.nodes.sizeInByte()) {
		Node* node = graph.nodeFromAbsOffset(offset);
		nodePtrs.pushBack(node);
		offset += node->sizeInBytes();
	}

	for(roSize i=0; i<nodePtrs.size(); ++i) {
		Node* node = nodePtrs[i];
		String s(node->debugStr.begin, node->debugStr.end - node->debugStr.begin);
		roLog("debug", "Node %d: %s\n", i, s.c_str());
		for(roSize j=0; j<node->edgeCount; ++j) {
			Node* nextNode = graph.followEdge(node->edge(j));
			roSize idx = nodePtrs.find(nextNode) - nodePtrs.typedPtr();
			roLog("debug", "  edge -> node %d\n", idx);
		}
	}
	roLog("debug", "\n");
}

bool Regex::match(const roUtf8* s, const roUtf8* f, const char* options)
{
	RangedString regString(f, f + roStrLen(f));
	RangedString srcString(s, s + roStrLen(s));

	Graph graph;
	graph.regex = this;
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

	if(isDebug) {
		roLog("debug", "%s, %s\n", f, s);
		debugNodes(graph);
	}

	result.resize(graph.resultEndNodeOffset.size() + 1);

	// Matching
	while(srcString.begin < srcString.end) {
		const roUtf8* currentBegin = srcString.begin;
		graph.branchLevel = 0;
		graph.nestedGroupLevel = 0;

		if(matchNodes(graph, (Node&)graph.nodes.front(), srcString)) {
			result[0] = RangedString(currentBegin, srcString.begin);

			for(roSize i=0; i<graph.resultEndNodeOffset.size(); ++i) {
				Node* node = graph.nodeFromAbsOffset(graph.resultEndNodeOffset[i]);
				RangedString str = RangedString((roUtf8*)node->userdata[1], (roUtf8*)node->userdata[2]);
				result[i + 1] = str;
			}

			return true;
		}

		// If no match found, try start at next character in the string
		srcString.begin = currentBegin + 1;
	}

	result.clear();
	return false;
}

}   // namespace ro
