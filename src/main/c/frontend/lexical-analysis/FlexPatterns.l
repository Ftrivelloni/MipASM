%top{

#include "../../support/type/CompilationStatus.h"
#include "../../support/type/FlexContext.h"
#include "../../support/type/TokenLabel.h"
#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include "../syntactic-analysis/BisonParser.h"

}

%{

extern CompilationStatus KeywordLexemeAction(TokenLabel label);
extern CompilationStatus PunctuationLexemeAction(TokenLabel label);
extern CompilationStatus OperatorLexemeAction(TokenLabel label);
extern CompilationStatus ArithmeticOperatorLexemeAction(TokenLabel label);
extern CompilationStatus ParenthesisLexemeAction(TokenLabel label);
extern CompilationStatus BraceLexemeAction(TokenLabel label);
extern CompilationStatus IdentifierLexemeAction();
extern CompilationStatus IntegerLiteralLexemeAction();
extern CompilationStatus FloatLiteralLexemeAction();
extern CompilationStatus StringLiteralLexemeAction();
extern CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
extern CompilationStatus LeaveMultilineCommentLexemeAction();
extern CompilationStatus IgnoredLexemeAction();
extern CompilationStatus EOFLexemeAction();
extern CompilationStatus UnknownLexemeAction();
extern CompilationStatus IncludeDirectiveLexemeAction();

/**
 * Runs at the start of every matched rule (after yytext/yyleng are set) to
 * fill Bison's location (yylloc) for the token. Flex already maintains the
 * line number (--yylineno); here we also track a 1-based column, resetting it
 * on every newline. Populating yylloc is what lets the parser, semantic
 * analyzer and generator report "line:column" diagnostics — without this the
 * location stays zero. Columns are per-buffer, so they reset across #includes.
 */
#define YY_USER_ACTION \
	yylloc->first_line = yylineno; \
	yylloc->first_column = yycolumn + 1; \
	for (int _column = 0; _column < yyleng; ++_column) { \
		if (yytext[_column] == '\n') { yycolumn = 0; } else { ++yycolumn; } \
	} \
	yylloc->last_line = yylineno; \
	yylloc->last_column = yycolumn;

%}

%x MULTILINE_COMMENT

whitespace      [[:space:]]+
line_comment    "//"[^\n]*

%%

	/* Multi-line comments. */
"/*"                        { return EnterMultilineCommentLexemeAction(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>"*/"     { return LeaveMultilineCommentLexemeAction(); }
<MULTILINE_COMMENT>[^*]+    { return IgnoredLexemeAction(); }
<MULTILINE_COMMENT>.        { return IgnoredLexemeAction(); }

	/* Line comments and whitespace. */
{line_comment}              { return IgnoredLexemeAction(); }
{whitespace}                { return IgnoredLexemeAction(); }

	/* Preprocessor directive. A well-formed include is consumed entirely
	   here: the library file's tokens are spliced in through the buffer
	   stack and no token reaches the parser. The bare keyword below only
	   fires for malformed directives, which then fail at parse time. */
"#include"[[:space:]]*\"<[a-zA-Z0-9_./]+>\"   { return IncludeDirectiveLexemeAction(); }
"#include"                  { return KeywordLexemeAction(INCLUDE); }

	/* Type keywords. */
"void"                      { return KeywordLexemeAction(VOID); }
"int"                       { return KeywordLexemeAction(INT_KW); }
"float"                     { return KeywordLexemeAction(FLOAT_KW); }
"boolean"                   { return KeywordLexemeAction(BOOLEAN_KW); }
"track"                     { return KeywordLexemeAction(TRACK_KW); }
"duration"                  { return KeywordLexemeAction(DURATION_KW); }

	/* Control keywords. */
"main"                      { return KeywordLexemeAction(MAIN); }
"if"                        { return KeywordLexemeAction(IF); }
"else"                      { return KeywordLexemeAction(ELSE); }
"for"                       { return KeywordLexemeAction(FOR); }
"while"                     { return KeywordLexemeAction(WHILE); }
"sync"                      { return KeywordLexemeAction(SYNC); }
"const"                     { return KeywordLexemeAction(CONST); }

	/* Domain keywords. */
"init_track"                { return KeywordLexemeAction(INIT_TRACK); }
"play"                      { return KeywordLexemeAction(PLAY); }
"rest"                      { return KeywordLexemeAction(REST); }
"set_volume"                { return KeywordLexemeAction(SET_VOLUME); }
"set_pan"                   { return KeywordLexemeAction(SET_PAN); }
"set_attack"                { return KeywordLexemeAction(SET_ATTACK); }
"set_sustain"               { return KeywordLexemeAction(SET_SUSTAIN); }
"set_modulation"            { return KeywordLexemeAction(SET_MODULATION); }
"set_reverb"                { return KeywordLexemeAction(SET_REVERB); }
"set_tempo"                 { return KeywordLexemeAction(SET_TEMPO); }
"set_instrument"            { return KeywordLexemeAction(SET_INSTRUMENT); }
"set_time_signature"        { return KeywordLexemeAction(SET_TIME_SIGNATURE); }

	/* Multi-character operators (must precede the single-char variants). */
"=="                        { return OperatorLexemeAction(EQ); }
"!="                        { return OperatorLexemeAction(NEQ); }
"<="                        { return OperatorLexemeAction(LEQ); }
">="                        { return OperatorLexemeAction(GEQ); }
"&&"                        { return OperatorLexemeAction(AND); }
"||"                        { return OperatorLexemeAction(OR); }

	/* Single-character relational and logical operators. */
"<"                         { return OperatorLexemeAction(LT); }
">"                         { return OperatorLexemeAction(GT); }
"!"                         { return OperatorLexemeAction(NOT); }
"="                         { return OperatorLexemeAction(ASSIGN); }

	/* Arithmetic operators. */
"+"                         { return ArithmeticOperatorLexemeAction(ADD); }
"-"                         { return ArithmeticOperatorLexemeAction(SUB); }
"*"                         { return ArithmeticOperatorLexemeAction(MUL); }
"/"                         { return ArithmeticOperatorLexemeAction(DIV); }

	/* Grouping and delimiters. */
"("                         { return ParenthesisLexemeAction(OPEN_PARENTHESIS); }
")"                         { return ParenthesisLexemeAction(CLOSE_PARENTHESIS); }
"{"                         { return BraceLexemeAction(OPEN_BRACE); }
"}"                         { return BraceLexemeAction(CLOSE_BRACE); }
","                         { return PunctuationLexemeAction(COMMA); }
";"                         { return PunctuationLexemeAction(SEMICOLON); }

	/* Literals. */
\"<[a-zA-Z0-9_./]+>\"       { return StringLiteralLexemeAction(); }
[0-9]*\.[0-9]+              { return FloatLiteralLexemeAction(); }
[0-9]+                      { return IntegerLiteralLexemeAction(); }

	/* Identifiers (must come after keywords). */
[a-zA-Z_][a-zA-Z0-9_]*      { return IdentifierLexemeAction(); }

	/* Fallback. */
.                           { return UnknownLexemeAction(); }
<<EOF>>                     { return EOFLexemeAction(); }

%%

#include "FlexActions.h"
#include "FlexExport.h"
