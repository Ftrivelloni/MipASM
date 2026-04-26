%{

#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonActions.h"

/**
 * The error reporting function for Bison parser.
 *
 * @todo Add location to the grammar and "pushToken" API function.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Error-Reporting-Function.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Tracking-Locations.html
 */
void yyerror(const YYLTYPE * location, const char * message) {}

%}

/// Bison declarations

// You touch this, and you die.
%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error detailed
%locations

%union {
	/** Terminals. */

	signed int integer;
	TokenLabel token;

	/** Non-terminals. */

	Constant * constant;
	Expression * expression;
	Factor * factor;
	Program * program;
}

/**
 * Destructors. This functions are executed after the parsing ends, so if the
 * AST must be used in the following phases of the compiler you shouldn't used
 * this approach for the AST root node ("program" non-terminal, in this
 * grammar), or it will drop the entire tree even if the parsing succeeds.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Destructor-Decl.html
 */
%destructor { destroyConstant($$); } <constant>       // estos son loa de %union
%destructor { destroyExpression($$); } <expression>  //
%destructor { destroyFactor($$); } <factor>         //
// no hay destructor de program

/** Terminals. */
// estos son lo que lee, onda como el codigo se levanta y pasa
%token <integer> INTEGER
%token <token> ADD
%token <token> SUB
%token <token> DIV
%token <token> MUL
%token <token> OPEN_BRACE // los braces en el coso este son para imports
%token <token> OPEN_COMMENT
%token <token> OPEN_PARENTHESIS
%token <token> CLOSE_BRACE // los braces son para imports ojo
%token <token> CLOSE_COMMENT
%token <token> CLOSE_PARENTHESIS

// falta agregarlas al lexer
%token <token> VOID
%token <token> MAIN

%token <token> IGNORED
%token <token> UNKNOWN

/** Non-terminals. */
%type <constant> constant
%type <expression> expression
%type <factor> factor
%type <program> program

/**
 * Precedence and associativity.
 *
 * @see https://en.cppreference.com/w/cpp/language/operator_precedence.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Precedence.html
 */
 //this marks presedence, * and / have higher precedence because they are declared later
%left ADD SUB
%left MUL DIV

%% /// grammar rules

// IMPORTANT: To use λ in the following grammar, use the %empty symbol.

//program: expression											{ $$ = ExpressionProgramSemanticAction($1); }
//	;
    ///   1		2		3					4			5			6			7						     Esto es por el orden VVVV		
program: VOID MAIN OPEN_PARENTHESIS CLOSE_PARENTHESIS OPEN_BRACE expression CLOSE_BRACE		{ $$ = ExpressionProgramSemanticAction($6) }
	;

expression: expression[left] ADD expression[right]			{ $$ = ArithmeticExpressionSemanticAction($left, $right, ADDITION); }
	| expression[left] DIV expression[right]				{ $$ = ArithmeticExpressionSemanticAction($left, $right, DIVISION); }
	| expression[left] MUL expression[right]				{ $$ = ArithmeticExpressionSemanticAction($left, $right, MULTIPLICATION); }
	| expression[left] SUB expression[right]				{ $$ = ArithmeticExpressionSemanticAction($left, $right, SUBTRACTION); }
	| factor												{ $$ = FactorExpressionSemanticAction($1); }
	;

factor: OPEN_PARENTHESIS expression CLOSE_PARENTHESIS		{ $$ = ExpressionFactorSemanticAction($2); }
	| constant												{ $$ = ConstantFactorSemanticAction($1); }
	;

constant: INTEGER											{ $$ = IntegerConstantSemanticAction($1); }
	;

%%
