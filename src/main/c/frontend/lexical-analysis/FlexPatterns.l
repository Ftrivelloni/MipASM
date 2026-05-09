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

	/* Preprocessor directive. */
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
