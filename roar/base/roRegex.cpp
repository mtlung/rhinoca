#include "pch.h"
#include "roRegex.h"
#include "roLog.h"

// Reference:
// Regular Expression Matching Can Be Simple And Fast
// http://swtch.com/~rsc/regexp/regexp1.html

namespace ro {

namespace {

static const char* symbols = "*+?|{}()[]^$";
static const char* escapes[] = { "nrtvf^*+?.|{}()[].", "\n\r\t\v\f^*+?.|{}()[]." };
static const char* shorcut = "sSbBdDwWnAZG";
static const char* repeatition = "?+*{";

static bool charCmp(char c1, char c2) { return c1 == c2; }
static bool charCaseCmp(char c1, char c2) { return roToLower(c1) == roToLower(c2); }

// Will eat '\' on successful escape
char doEscape(const roUtf8*& str)
{
	if(*str != '\\') return *str;
	const char* i = roStrChr(escapes[0], str[1]);
	if(!i) return '\0';
	return ++str, escapes[1][i - escapes[0]];
}

// Will eat '\' on successful scan
bool scanMeta(const roUtf8*& str, char c)
{
	if(*str != '\\') return *str == c;
	const char* i = roStrChr(escapes[0], str[1]);
	if(!i) return false;
	if(escapes[1][i - escapes[0]] == c)
		return ++str, true;
	return false;
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
	void* userdata[3];
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

	Edge* findEdge(Node& node)
	{
		for(roSize i=0; i<edgeCount; ++i) {
			Edge& e = (&edges)[i];
			if(reinterpret_cast<Node*>(((roByte*)&e) + e.nextNode) == &node)
				return &e;
		}
		return NULL;
	}

	bool directEdgeToNode(Edge& edge, Node& node)
	{
		bool isValidEdge = isValid(edge);
		roAssert(isValidEdge);
		if(!isValidEdge) return false;
		edge.nextNode = (roByte*)(&node) - (roByte*)(&edge);
		return true;
	}

	bool redirect(Node& from, Node& to)
	{
		Edge* edge = findEdge(from);
		if(!edge) return false;
		return directEdgeToNode(*edge, to);
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
	roSize max = (roSize)node.userdata[1];
	roSize& count = (roSize&)node.userdata[2];

	if(count < max)
		return ++count, loop_repeat(graph, node, edge, s);
	else
		return false;
}

bool counted_loop_exit(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	roSize min = (roSize)node.userdata[0];
	roSize& count = (roSize&)node.userdata[2];

	if(count < min)
		return count = 0, false;
	else
		return count = 0, true;
}

bool node_begin(Graph& graph, Node& node, Edge& edge, RangedString& s) { return true; }
bool node_end(Graph& graph, Node& node, Edge& edge, RangedString& s) { return true; }
bool pass_though(Graph& graph, Node& node, Edge& edge, RangedString& s) { return true; }
bool alternation(Graph& graph, Node& node, Edge& edge, RangedString& s) { return true; }

bool parse_nodes(Graph& graph, const RangedString& f);

struct Graph
{
	Graph()
		: regex(NULL)
		, nestedGroupLevel(0)
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

	Regex* regex;
	Array<roByte> nodes;
	Node* currentNode;
	Node* endNode;
	roSize nestedGroupLevel;
	RangedString regString;
	RangedString srcString;
	bool (*charCmpFunc)(char c1, char c2);
	Array<const roUtf8*> tmpResult;
};	// Graph

bool group_begin(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	edge.f.begin = s.begin;
	graph.tmpResult[(roSize)node.userdata[0]] = s.begin;
	return ++graph.nestedGroupLevel, true;
}
bool group_end(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	roAssert(graph.nestedGroupLevel > 0);
	roSize idx = (roSize)node.userdata[0];
	graph.regex->result[idx] = RangedString(graph.tmpResult[idx], s.begin);
	return --graph.nestedGroupLevel, true;
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

bool match_charClass(Graph& graph, Node& node, Edge& edge, RangedString& s)
{
	const roUtf8* i = edge.f.begin;

	roUtf8 c = *s.begin;
	bool exclusion= false;
	if(*i == '^')
		exclusion = true, ++i;

	bool inList = false;
	for(; i<edge.f.end; ++i) {
		// Character range
		if(i[1] == '-' && (i + 2) != edge.f.end) {
			roUtf8 l = roToLower(c);
			roUtf8 u = roToUpper(c);
			if((l >= i[0] && l <= i[2]) || (u >= i[0] && u <= i[2])) {
				inList = true;
				break;
			}
		}
		// Single character
		else if(graph.charCmpFunc(c, i[0])) {
			inList = true;
			break;
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

bool parse_repeatition(Graph& graph, Node* prevNode, Node* beginNode, Node* endNode, const RangedString& f, const roUtf8*& i)
{
	char escaped = doEscape(i);
	if(escaped == '?') {
		Edge edge = { RangedString(), pass_though, 0 };
		Edge* newEdge = graph.pushEdge(beginNode, edge, 1, &endNode);
		roVerify(beginNode->directEdgeToNode(*newEdge, *endNode));
	}
	else if(escaped == '+') {
		Node node = { 2, RangedString("+") };
		Node* loopNode = graph.push(node, 3, &prevNode, &beginNode, &endNode);
		loopNode->edge(0).func = loop_repeat;
		roVerify(loopNode->directEdgeToNode(loopNode->edge(0), *beginNode));

		loopNode->edge(1).func = loop_exit;
	}
	else if(escaped == '*') {
		Node node = { 2, RangedString("*") };
		Node* loopNode = graph.push(node, 3, &prevNode, &beginNode, &endNode);
		loopNode->edge(0).func = loop_repeat;
		roVerify(loopNode->directEdgeToNode(loopNode->edge(0), *beginNode));

		loopNode->edge(1).func = loop_exit;
		roVerify(prevNode->redirect(*beginNode, *loopNode));
	}
	else if(escaped == '{') {
		roSize min = 0, max = 0;
		int ss = sscanf(i, "{%u,%u", &min, &max);
		if(ss == 0)
			return false;

		// Scan for '}'
		while(*(++i) != '}') {
			if(i >= f.end)
				return false;
		}

		Node node = { 2, RangedString("*") };
		Node* loopNode = graph.push(node, 3, &prevNode, &beginNode, &endNode);
		loopNode->edge(0).func = counted_loop_repeat;
		roVerify(loopNode->directEdgeToNode(loopNode->edge(0), *beginNode));

		loopNode->edge(1).func = counted_loop_exit;
		roVerify(prevNode->redirect(*beginNode, *loopNode));

		if(min > max) max = min;
		loopNode->userdata[0] = (void*)min;
		loopNode->userdata[1] = (void*)max;
		loopNode->userdata[2] = (void*)0;
	}
	else
		return true;

	++i;
	return true;
}

bool parse_raw(Graph& graph, const RangedString& f, const roUtf8*& i)
{
	if(roStrChr(symbols, *i))
		return false;

	// Search for end of raw string
	const roUtf8* end2 = i;
	for(; end2 < f.end; ++end2) {
		if(roStrChr(symbols, *end2))
			break;
	}

	roAssert(end2 != i);
	const roUtf8* end1 = end2;

	// See if any repetition follow the string, if yes we divide the string into 2 parts
	if(end2 != f.end && roStrChr(repeatition, *end2))
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

bool parse_charClass(Graph& graph, const RangedString& f, const roUtf8*& i)
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
	node.edges.func = match_charClass;

	Node* prevNode = graph.currentNode;
	Node* beginNode = graph.push(node, 1, &prevNode);
	Node* endNode = graph.endNode;

	parse_repeatition(graph, prevNode, beginNode, endNode, f, i);

	return true;
}

bool parse_group(Graph& graph, const RangedString& f, const roUtf8*& i)
{
	if(*i != '(') return false;
	const roUtf8* begin = ++i;

	// Look for corresponding ')'
	for(roSize parenthesesCount = 1; true; ++i) {
		char escaped = doEscape(i);
		parenthesesCount += (escaped == '(') ? 1 : 0;
		parenthesesCount -= (escaped == ')') ? 1 : 0;
		if(i >= f.end) return false;
		if(parenthesesCount == 0) break;
	}

	const roUtf8* end = i;
	roAssert(end > begin);

	roSize prevNodeOffset, beginNodeOffset;
	roSize groupIndex = graph.regex->result.size();

	{	// Add starting node
		Node beginNode = { 1, RangedString("(") };
		beginNode.edges.func = group_begin;
		prevNodeOffset = graph.absOffsetFromNode(*graph.currentNode);
		beginNodeOffset = graph.absOffsetFromNode(*graph.push(beginNode));

		graph.currentNode->userdata[0] = (void*)groupIndex;
	}

	if(!parse_nodes(graph, RangedString(begin, end)))
		return false;

	{	// Add end node
		Node endNode = { 1, RangedString(")") };
		endNode.edges.func = group_end;
		graph.push(endNode);

		graph.currentNode->userdata[0] = (void*)groupIndex;
		graph.regex->result.incSize(1);
		graph.tmpResult.incSize(1);
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
		ret |= b = parse_raw(graph, f, i);					if(b) continue;
		ret |= b = parse_charClass(graph, f, i);			if(b) continue;
		ret |= b = parse_group(graph, f, i);				if(b) continue;
		ret |= b = parse_match_beginOfString(graph, f, i);	if(b) continue;
		ret |= b = parse_match_endOfString(graph, f, i);	if(b) continue;
		break;
	}

	return ret;
}

bool parse_nodes(Graph& graph, const RangedString& f)
{
	roSize bracketCount = 0;
	const roUtf8* i = f.begin;
	const roUtf8* begin = f.begin;
	TinyArray<roSize, 16> absOffsets;

	roSize beginNodeOffset = graph.absOffsetFromNode(*graph.currentNode);

	// Scan till next '|' or till end
	while(true) {
		if( bracketCount == 0 &&
			((i[0] == '|' && i[-1] != '\\') || i == f.end)
		)
		{
			if(!parse_nodes_impl(graph, RangedString(begin, i)))
				return false;

			begin = i + 1;
			absOffsets.pushBack(graph.absOffsetFromNode(*graph.currentNode));
		}

		bracketCount += scanMeta(i, '(') ? 1 : 0;
		bracketCount -= scanMeta(i, ')') ? 1 : 0;

		if((i++) == f.end)
			break;
	}

	// If there were alternation, adjust the edges
	for(roSize j=1; j<absOffsets.size(); ++j) {
		Node* node1 = graph.nodeFromAbsOffset(absOffsets[j-1]);
		Node* node2 = graph.nodeFromAbsOffset(absOffsets[j-0]);
		roVerify(node1->redirect(*node2, *graph.endNode));
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
			return true;

		if(!(*edge.func)(graph, *pNode, edge, s))
			return false;

		pNode = graph.followEdge(edge);
	}

	// Call recursively if there is branching
	for(roSize i=0; i<pNode->edgeCount; ++i) {
		const roUtf8* sBackup = s.begin;
		Edge& edge = (&pNode->edges)[i];
		if(edge.func == node_end)
			return true;

		if((*edge.func)(graph, *pNode, edge, s)) {
			Node* nextNode = graph.followEdge(edge);
			if(!nextNode)
				continue;
			if(matchNodes(graph, *nextNode, s))
				return true;
		}

		// Edge fail, restore the string and try another edge
		s.begin = sBackup;
	}

	return false;
}

void debugNodes(Graph& graph)
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
			roLog("debug", "  edge %d -> node %d\n", j, idx);
		}
	}
	roLog("debug", "\n");
}

Regex::Regex()
	: isDebug(false)
{
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
	graph.tmpResult.clear();
	parse_nodes(graph, regString);
	if(isDebug)
		debugNodes(graph);

	// Matching
	if(matchNodes(graph, (Node&)graph.nodes.front(), srcString)) {
		result.insert(0, RangedString(graph.srcString.begin, srcString.begin));
		return true;
	}

	result.clear();
	return false;
}

}   // namespace ro
