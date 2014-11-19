#include "pch.h"
#include "../../roar/base/roLexer.h"
#include "../../roar/base/roRegex.h"

using namespace ro;

class LexerTest {};

TEST_FIXTURE(LexerTest, basic)
{
	Lexer lexer;

	struct LocalScope {
		static bool matchComment(RangedString& inout, void* userData)
		{
			RangedString org = inout;
			for(roSize openCount = 0; inout.size() >= 2; ) {
				bool foundOpen  = (inout[0] == '/' && inout[1] == '*');
				bool foundClose = (inout[0] == '*' && inout[1] == '/');
				inout.begin += (foundOpen || foundClose) ? 2 : 1;

				openCount += foundOpen ? 1 : 0;
				openCount -= foundClose ? 1 : 0;

				if(openCount == 0)
					break;
			}
			
			const char min[] = "/**/";
			(void)min;	// Suppress warning
			return inout.begin - org.begin >= (sizeof(min) - 1);
		}
	};
	lexer.registerCustomRule("Comment", LocalScope::matchComment, NULL);

	lexer.registerRegexRule("WhiteSpace",
		"[ \t\n\r]+");

	lexer.registerRegexRule("StringLiteral",
		"L?\"(\\.|[^\"])*\"");

	lexer.registerRegexRule("FloatExponent",
		"[eE][+-]?[0-9]+", "", true);

	lexer.registerRegexRule("FloatLiteral",
		"([0-9]+\\.[0-9]*){FloatExponent}?|"
		"([0-9]+){FloatExponent}?|"
		"\\.[0-9]+{FloatExponent}?"
	);

	lexer.registerRegexRule("Comment",
		"//[^\r\n]*");

	lexer.registerRegexRule("Identifier",
		"[a-zA-Z_][a-zA-Z0-9_]*");

	lexer.registerRegexRule("=",	"=");
	lexer.registerRegexRule("==",	"==");
	lexer.registerRegexRule("(",	"\\(");
	lexer.registerRegexRule(")",	"\\)");

	// https://developer.apple.com/library/prerelease/ios/documentation/Swift/Conceptual/Swift_Programming_Language/LexicalStructure.html
	// https://developer.apple.com/library/prerelease/ios/documentation/Swift/Conceptual/Swift_Programming_Language/zzSummaryOfTheGrammar.html
	// Keywords used in declarations
	lexer.registerRegexRule("class",		"class");
	lexer.registerRegexRule("deinit",		"deinit");
	lexer.registerRegexRule("enum",			"enum");
	lexer.registerRegexRule("extension",	"extension");
	lexer.registerRegexRule("func",			"func");
	lexer.registerRegexRule("import",		"import");
	lexer.registerRegexRule("init",			"init");
	lexer.registerRegexRule("let",			"let");
	lexer.registerRegexRule("protocol",		"protocol");
	lexer.registerRegexRule("static",		"static");
	lexer.registerRegexRule("struct",		"struct");
	lexer.registerRegexRule("subscript",	"subscript");
	lexer.registerRegexRule("typealias",	"typealias");
	lexer.registerRegexRule("var",			"var");

	// Keywords used in statements
	lexer.registerRegexRule("break",		"break");
	lexer.registerRegexRule("case",			"case");
	lexer.registerRegexRule("continue",		"continue");
	lexer.registerRegexRule("default",		"default");
	lexer.registerRegexRule("do",			"do");
	lexer.registerRegexRule("else",			"else");
	lexer.registerRegexRule("fallthrough",	"fallthrough");
	lexer.registerRegexRule("if",			"if");
	lexer.registerRegexRule("in",			"in");
	lexer.registerRegexRule("for",			"for");
	lexer.registerRegexRule("return",		"return");
	lexer.registerRegexRule("switch",		"switch");
	lexer.registerRegexRule("where",		"where");
	lexer.registerRegexRule("while",		"while");

	// Keywords used in expressions and types
	lexer.registerRegexRule("as",			"as");
	lexer.registerRegexRule("dynamicType",	"dynamicType");
	lexer.registerRegexRule("is",			"is");
	lexer.registerRegexRule("new",			"new");
	lexer.registerRegexRule("super",		"super");
	lexer.registerRegexRule("self",			"self");
	lexer.registerRegexRule("Self",			"Self");
	lexer.registerRegexRule("Type",			"Type");
	lexer.registerRegexRule("__COLUMN__",	"__COLUMN__");
	lexer.registerRegexRule("__FILE__",		"__FILE__");
	lexer.registerRegexRule("__FUNCTION__",	"__FUNCTION__");
	lexer.registerRegexRule("__LINE__",		"__LINE__");

	// Key words
	lexer.registerRegexRule("int",			"int");
	lexer.registerRegexRule("double",		"double");

	String source(
		"/**//*multiline \ncomment*/ double a=4e2 if(a==0.123) b=4.7e2 \"hello\" // Comment"
	);

	CHECK(lexer.beginParse(source));

	Lexer::Token token;
	while(lexer.nextToken(token))
	{
		token = token;
	}

	CHECK(lexer.endParse());
}

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
	tok_eof = -1,

	// commands
	tok_def = -2, tok_extern = -3,

	// primary
	tok_identifier = -4, tok_number = -5
};

static std::string IdentifierStr;  // Filled in if tok_identifier
static double NumVal;              // Filled in if tok_number

/// gettok - Return the next token from standard input.
static int gettok() {
	static int LastChar = ' ';

	// Skip any whitespace.
	while (isspace(LastChar))
		LastChar = getchar();

	if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
		IdentifierStr = LastChar;
		while (isalnum((LastChar = getchar())))
			IdentifierStr += LastChar;

		if (IdentifierStr == "def") return tok_def;
		if (IdentifierStr == "extern") return tok_extern;
		return tok_identifier;
	}

	if (isdigit(LastChar) || LastChar == '.') {   // Number: [0-9.]+
		std::string NumStr;
		do {
			NumStr += LastChar;
			LastChar = getchar();
		} while (isdigit(LastChar) || LastChar == '.');

		NumVal = strtod(NumStr.c_str(), 0);
		return tok_number;
	}

	if (LastChar == '#') {
		// Comment until end of line.
		do LastChar = getchar();
		while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

		if (LastChar != EOF)
			return gettok();
	}

	// Check for end of file.  Don't eat the EOF.
	if (LastChar == EOF)
		return tok_eof;

	// Otherwise, just return the character as its ascii value.
	int ThisChar = LastChar;
	LastChar = getchar();
	return ThisChar;
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//
namespace {
	/// ExprAST - Base class for all expression nodes.
	class ExprAST {
	public:
		virtual ~ExprAST() {}
	};

	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST {
	public:
		NumberExprAST(double val) {}
	};

	/// VariableExprAST - Expression class for referencing a variable, like "a".
	class VariableExprAST : public ExprAST {
		std::string Name;
	public:
		VariableExprAST(const std::string &name) : Name(name) {}
	};

	/// BinaryExprAST - Expression class for a binary operator.
	class BinaryExprAST : public ExprAST {
	public:
		BinaryExprAST(char op, ExprAST *lhs, ExprAST *rhs) {}
	};

	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST {
		std::string Callee;
		std::vector<ExprAST*> Args;
	public:
		CallExprAST(const std::string &callee, std::vector<ExprAST*> &args)
			: Callee(callee), Args(args) {}
	};

	/// PrototypeAST - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes).
	class PrototypeAST {
		std::string Name;
		std::vector<std::string> Args;
	public:
		PrototypeAST(const std::string &name, const std::vector<std::string> &args)
			: Name(name), Args(args) {}

	};

	/// FunctionAST - This class represents a function definition itself.
	class FunctionAST {
	public:
		FunctionAST(PrototypeAST *proto, ExprAST *body) {}
	};
} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static int CurTok;
static int getNextToken() {
	return CurTok = gettok();
}

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
	if (!isascii(CurTok))
		return -1;

	// Make sure it's a declared binop.
	int TokPrec = BinopPrecedence[CurTok];
	if (TokPrec <= 0) return -1;
	return TokPrec;
}

/// Error* - These are little helper functions for error handling.
ExprAST *Error(const char *Str) { fprintf(stderr, "Error: %s\n", Str);return 0;}
PrototypeAST *ErrorP(const char *Str) { Error(Str); return 0; }

static ExprAST *ParseExpression();

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static ExprAST *ParseIdentifierExpr() {
	std::string IdName = IdentifierStr;

	getNextToken();  // eat identifier.

	if (CurTok != '(') // Simple variable ref.
		return new VariableExprAST(IdName);

	// Call.
	getNextToken();  // eat (
	std::vector<ExprAST*> Args;
	if (CurTok != ')') {
		while (1) {
			ExprAST *Arg = ParseExpression();
			if (!Arg) return 0;
			Args.push_back(Arg);

			if (CurTok == ')') break;

			if (CurTok != ',')
				return Error("Expected ')' or ',' in argument list");
			getNextToken();
		}
	}

	// Eat the ')'.
	getNextToken();

	return new CallExprAST(IdName, Args);
}

/// numberexpr ::= number
static ExprAST *ParseNumberExpr() {
	ExprAST *Result = new NumberExprAST(NumVal);
	getNextToken(); // consume the number
	return Result;
}

/// parenexpr ::= '(' expression ')'
static ExprAST *ParseParenExpr() {
	getNextToken();  // eat (.
	ExprAST *V = ParseExpression();
	if (!V) return 0;

	if (CurTok != ')')
		return Error("expected ')'");
	getNextToken();  // eat ).
	return V;
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
static ExprAST *ParsePrimary() {
	switch (CurTok) {
	default: return Error("unknown token when expecting an expression");
	case tok_identifier: return ParseIdentifierExpr();
	case tok_number:     return ParseNumberExpr();
	case '(':            return ParseParenExpr();
	}
}

/// binoprhs
///   ::= ('+' primary)*
static ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS) {
	// If this is a binop, find its precedence.
	while (1) {
		int TokPrec = GetTokPrecedence();

		// If this is a binop that binds at least as tightly as the current binop,
		// consume it, otherwise we are done.
		if (TokPrec < ExprPrec)
			return LHS;

		// Okay, we know this is a binop.
		int BinOp = CurTok;
		getNextToken();  // eat binop

		// Parse the primary expression after the binary operator.
		ExprAST *RHS = ParsePrimary();
		if (!RHS) return 0;

		// If BinOp binds less tightly with RHS than the operator after RHS, let
		// the pending operator take RHS as its LHS.
		int NextPrec = GetTokPrecedence();
		if (TokPrec < NextPrec) {
			RHS = ParseBinOpRHS(TokPrec+1, RHS);
			if (RHS == 0) return 0;
		}

		// Merge LHS/RHS.
		LHS = new BinaryExprAST(BinOp, LHS, RHS);
	}
}

/// expression
///   ::= primary binoprhs
///
static ExprAST *ParseExpression() {
	ExprAST *LHS = ParsePrimary();
	if (!LHS) return 0;

	return ParseBinOpRHS(0, LHS);
}

/// prototype
///   ::= id '(' id* ')'
static PrototypeAST *ParsePrototype() {
	if (CurTok != tok_identifier)
		return ErrorP("Expected function name in prototype");

	std::string FnName = IdentifierStr;
	getNextToken();

	if (CurTok != '(')
		return ErrorP("Expected '(' in prototype");

	std::vector<std::string> ArgNames;
	while (getNextToken() == tok_identifier)
		ArgNames.push_back(IdentifierStr);
	if (CurTok != ')')
		return ErrorP("Expected ')' in prototype");

	// success.
	getNextToken();  // eat ')'.

	return new PrototypeAST(FnName, ArgNames);
}

/// definition ::= 'def' prototype expression
static FunctionAST *ParseDefinition() {
	getNextToken();  // eat def.
	PrototypeAST *Proto = ParsePrototype();
	if (Proto == 0) return 0;

	if (ExprAST *E = ParseExpression())
		return new FunctionAST(Proto, E);
	return 0;
}

/// toplevelexpr ::= expression
static FunctionAST *ParseTopLevelExpr() {
	if (ExprAST *E = ParseExpression()) {
		// Make an anonymous proto.
		PrototypeAST *Proto = new PrototypeAST("", std::vector<std::string>());
		return new FunctionAST(Proto, E);
	}
	return 0;
}

/// external ::= 'extern' prototype
static PrototypeAST *ParseExtern() {
	getNextToken();  // eat extern.
	return ParsePrototype();
}

//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

static void HandleDefinition() {
	if (ParseDefinition()) {
		fprintf(stderr, "Parsed a function definition.\n");
	} else {
		// Skip token for error recovery.
		getNextToken();
	}
}

static void HandleExtern() {
	if (ParseExtern()) {
		fprintf(stderr, "Parsed an extern\n");
	} else {
		// Skip token for error recovery.
		getNextToken();
	}
}

static void HandleTopLevelExpression() {
	// Evaluate a top-level expression into an anonymous function.
	if (ParseTopLevelExpr()) {
		fprintf(stderr, "Parsed a top-level expr\n");
	} else {
		// Skip token for error recovery.
		getNextToken();
	}
}

/// top ::= definition | external | expression | ';'
static void MainLoop() {
	while (1) {
		fprintf(stderr, "ready> ");
		switch (CurTok) {
		case tok_eof:    return;
		case ';':        getNextToken(); break;  // ignore top-level semicolons.
		case tok_def:    HandleDefinition(); break;
		case tok_extern: HandleExtern(); break;
		default:         HandleTopLevelExpression(); break;
		}
	}
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

TEST_FIXTURE(LexerTest, parsing)
{
	// Install standard binary operators.
	// 1 is lowest precedence.
	BinopPrecedence['<'] = 10;
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 20;
	BinopPrecedence['*'] = 40;  // highest.

	// Prime the first token.
	fprintf(stderr, "ready> ");
	getNextToken();

	// Run the main "interpreter loop" now.
	MainLoop();
}