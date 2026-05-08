%code requires {

#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"

}

%{

#include "BisonActions.h"

/**
 * The error reporting function for Bison parser.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Error-Reporting-Function.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Tracking-Locations.html
 */
void yyerror(const YYLTYPE * location, const char * message) {
	(void) location;
	(void) message;
}

%}

/* Bison declarations. */

// You touch this, and you die.
%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error detailed
%locations

%union {
	/** Terminals. */
	signed int integer;
	float floatValue;
	char * string;
	TokenLabel token;

	/** Non-terminals. */
	ASTNode * node;
	ASTList * list;
	TypeKind typeKind;
}

/* Keywords. */
%token <token> VOID
%token <token> MAIN
%token <token> INT_KW
%token <token> FLOAT_KW
%token <token> BOOLEAN_KW
%token <token> TRACK_KW
%token <token> DURATION_KW
%token <token> IF
%token <token> ELSE
%token <token> FOR
%token <token> WHILE
%token <token> SYNC
%token <token> CONST
%token <token> INIT_TRACK
%token <token> PLAY
%token <token> REST
%token <token> SET_VOLUME
%token <token> SET_PAN
%token <token> SET_ATTACK
%token <token> INCLUDE

/* Operators. */
%token <token> ADD SUB MUL DIV
%token <token> EQ NEQ LT GT LEQ GEQ
%token <token> AND OR NOT
%token <token> ASSIGN

/* Delimiters. */
%token <token> OPEN_PARENTHESIS CLOSE_PARENTHESIS
%token <token> OPEN_BRACE CLOSE_BRACE
%token <token> COMMA SEMICOLON

/* Internal/diagnostic tokens used by the lexer for logging. */
%token <token> OPEN_COMMENT CLOSE_COMMENT
%token <token> IGNORED UNKNOWN

/* Literals. */
%token <integer>    INT_LIT
%token <floatValue> FLOAT_LIT
%token <string>     ID
%token <string>     STRING_LIT

/* Non-terminals. */
%type <node> program main_func block include_stmt
%type <node> declaration assignment track_init global_declaration
%type <node> statement play_stmt rest_stmt sync_block cc_stmt
%type <node> control_stmt if_stmt for_stmt while_stmt
%type <node> expression
%type <list> directives global_declarations statements
%type <typeKind> type

/*
 * No %destructor directives: when the parse succeeds, the AST root is owned
 * by CompilerState (set in ProgramSemanticAction) and freed by EntryPoint
 * via destroyASTNode. Bison destructors at YYACCEPT would race against that
 * ownership and produce double-frees / use-after-free.
 */

/**
 * Precedence and associativity (lowest to highest).
 * Multiplication and division bind tighter because they are declared later.
 *
 * @see https://en.cppreference.com/w/cpp/language/operator_precedence.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Precedence.html
 */
%right ASSIGN
%left OR
%left AND
%left EQ NEQ
%left LT LEQ GT GEQ
%left ADD SUB
%left MUL DIV
%right NOT
%nonassoc THEN_PREC
%nonassoc ELSE

%start program

%%

/* Grammar rules. */

// IMPORTANT: To use lambda in the following grammar, use the %empty symbol.
// The numbers in semantic actions are Bison's rule positions. Count slowly, suffer less.

program
	: directives global_declarations main_func
		{ $$ = ProgramSemanticAction($1, $2, $3); }
	;

directives
	: include_stmt directives
		{ $$ = PrependASTListSemanticAction($1, $2); }
	| %empty
		{ $$ = NULL; }
	;

include_stmt
	: INCLUDE STRING_LIT
		{ $$ = IncludeSemanticAction($2); }
	;

global_declarations
	: global_declaration global_declarations
		{ $$ = PrependASTListSemanticAction($1, $2); }
	| %empty
		{ $$ = NULL; }
	;

global_declaration
	: declaration   { $$ = $1; }
	| track_init    { $$ = $1; }
	;

main_func
	: VOID MAIN OPEN_PARENTHESIS CLOSE_PARENTHESIS block
		{ $$ = MainFuncSemanticAction($5); }
	;

block
	: OPEN_BRACE statements CLOSE_BRACE
		{ $$ = BlockSemanticAction($2); }
	;

statements
	: statement statements
		{ $$ = PrependASTListSemanticAction($1, $2); }
	| %empty
		{ $$ = NULL; }
	;

statement
	: declaration            { $$ = $1; }
	| assignment SEMICOLON   { $$ = $1; }
	| track_init             { $$ = $1; }
	| play_stmt              { $$ = $1; }
	| rest_stmt              { $$ = $1; }
	| sync_block             { $$ = $1; }
	| control_stmt           { $$ = $1; }
	| cc_stmt                { $$ = $1; }
	;

declaration
	: type ID SEMICOLON
		{ $$ = DeclarationSemanticAction(0, $1, $2, NULL); }
	| type ID ASSIGN expression SEMICOLON
		{ $$ = DeclarationSemanticAction(0, $1, $2, $4); }
	| CONST type ID ASSIGN expression SEMICOLON
		{ $$ = DeclarationSemanticAction(1, $2, $3, $5); }
	;

type
	: INT_KW       { $$ = TYPE_INT; }
	| FLOAT_KW     { $$ = TYPE_FLOAT; }
	| BOOLEAN_KW   { $$ = TYPE_BOOLEAN; }
	| DURATION_KW  { $$ = TYPE_DURATION; }
	;

assignment
	: ID ASSIGN expression
		{ $$ = AssignmentSemanticAction($1, $3); }
	;

track_init
	: TRACK_KW ID ASSIGN INIT_TRACK OPEN_PARENTHESIS expression CLOSE_PARENTHESIS SEMICOLON
		{ $$ = TrackInitSemanticAction($2, $6); }
	;

play_stmt
	: PLAY OPEN_PARENTHESIS ID COMMA expression COMMA expression CLOSE_PARENTHESIS SEMICOLON
		{ $$ = PlaySemanticAction($3, $5, $7); }
	;

rest_stmt
	: REST OPEN_PARENTHESIS ID COMMA expression CLOSE_PARENTHESIS SEMICOLON
		{ $$ = RestSemanticAction($3, $5); }
	;

sync_block
	: SYNC block
		{ $$ = SyncBlockSemanticAction($2); }
	;

cc_stmt
	: SET_VOLUME OPEN_PARENTHESIS ID COMMA expression CLOSE_PARENTHESIS SEMICOLON
		{ $$ = CCStmtSemanticAction(CC_VOLUME, $3, $5); }
	| SET_PAN OPEN_PARENTHESIS ID COMMA expression CLOSE_PARENTHESIS SEMICOLON
		{ $$ = CCStmtSemanticAction(CC_PAN, $3, $5); }
	| SET_ATTACK OPEN_PARENTHESIS ID COMMA expression CLOSE_PARENTHESIS SEMICOLON
		{ $$ = CCStmtSemanticAction(CC_ATTACK, $3, $5); }
	;

control_stmt
	: if_stmt    { $$ = $1; }
	| for_stmt   { $$ = $1; }
	| while_stmt { $$ = $1; }
	;

if_stmt
	: IF OPEN_PARENTHESIS expression CLOSE_PARENTHESIS block %prec THEN_PREC
		{ $$ = IfSemanticAction($3, $5, NULL); }
	| IF OPEN_PARENTHESIS expression CLOSE_PARENTHESIS block ELSE block
		{ $$ = IfSemanticAction($3, $5, $7); }
	;

for_stmt
	: FOR OPEN_PARENTHESIS type ID ASSIGN expression SEMICOLON expression SEMICOLON assignment CLOSE_PARENTHESIS block
		{ $$ = ForSemanticAction(1, $3, $4, $6, $8, $10, $12); }
	| FOR OPEN_PARENTHESIS ID ASSIGN expression SEMICOLON expression SEMICOLON assignment CLOSE_PARENTHESIS block
		{ $$ = ForSemanticAction(0, TYPE_INT, $3, $5, $7, $9, $11); }
	;

while_stmt
	: WHILE OPEN_PARENTHESIS expression CLOSE_PARENTHESIS block
		{ $$ = WhileSemanticAction($3, $5); }
	;

expression
	: expression ADD expression  { $$ = BinaryOpSemanticAction(OP_ADD, $1, $3); }
	| expression SUB expression  { $$ = BinaryOpSemanticAction(OP_SUB, $1, $3); }
	| expression MUL expression  { $$ = BinaryOpSemanticAction(OP_MUL, $1, $3); }
	| expression DIV expression  { $$ = BinaryOpSemanticAction(OP_DIV, $1, $3); }
	| expression EQ  expression  { $$ = BinaryOpSemanticAction(OP_EQ,  $1, $3); }
	| expression NEQ expression  { $$ = BinaryOpSemanticAction(OP_NEQ, $1, $3); }
	| expression LT  expression  { $$ = BinaryOpSemanticAction(OP_LT,  $1, $3); }
	| expression GT  expression  { $$ = BinaryOpSemanticAction(OP_GT,  $1, $3); }
	| expression LEQ expression  { $$ = BinaryOpSemanticAction(OP_LEQ, $1, $3); }
	| expression GEQ expression  { $$ = BinaryOpSemanticAction(OP_GEQ, $1, $3); }
	| expression AND expression  { $$ = BinaryOpSemanticAction(OP_AND, $1, $3); }
	| expression OR  expression  { $$ = BinaryOpSemanticAction(OP_OR,  $1, $3); }
	| NOT expression             { $$ = UnaryOpSemanticAction(OP_NOT, $2); }
	| OPEN_PARENTHESIS expression CLOSE_PARENTHESIS
		{ $$ = ParenSemanticAction($2); }
	| ID         { $$ = IdentifierSemanticAction($1); }
	| INT_LIT    { $$ = IntLitSemanticAction($1); }
	| FLOAT_LIT  { $$ = FloatLitSemanticAction($1); }
	;

%%
