#ifndef ABSTRACT_SYNTAX_TREE_HEADER
#define ABSTRACT_SYNTAX_TREE_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdbool.h>
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeAbstractSyntaxTreeModule();

/**
 * This type definition allows the AST to be self-referencing: blocks contain
 * lists of AST nodes, expressions contain other expressions, and control-flow
 * nodes contain nested blocks, such as talking about you in 3rd person, but
 * without the madness.
 */
typedef struct ASTNode ASTNode;
typedef struct ASTList ASTList;

/**
 * Node types for the Abstract Syntax Tree (AST).
 */
typedef enum {
	AST_PROGRAM,
	AST_INCLUDE,
	AST_MAIN_FUNC,
	AST_BLOCK,

	AST_DECLARATION,
	AST_TRACK_INIT,

	AST_ASSIGNMENT,
	AST_PLAY,
	AST_REST,
	AST_SYNC_BLOCK,
	AST_CC_STMT,

	AST_IF,
	AST_FOR,
	AST_WHILE,

	AST_BINARY_OP,
	AST_UNARY_OP,
	AST_INT_LIT,
	AST_FLOAT_LIT,
	AST_IDENTIFIER,
	AST_PAREN_EXPR
} NodeType;

typedef enum {
	OP_ADD, OP_SUB, OP_MUL, OP_DIV,
	OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LEQ, OP_GEQ,
	OP_AND, OP_OR, OP_NOT
} OperatorKind;

typedef enum {
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_BOOLEAN,
	TYPE_TRACK,
	TYPE_DURATION
} TypeKind;

typedef enum {
	CC_VOLUME,
	CC_PAN,
	CC_ATTACK
} CCKind;

struct ASTList {
	ASTNode * node;
	ASTList * next;
};

struct ASTNode {
	NodeType nodeType;
	union {
		struct {
			ASTList * includes;
			ASTList * globalDecls;
			ASTNode * mainFunc;
		} program;

		struct {
			char * path;
		} include;

		struct {
			ASTNode * body;
		} mainFunc;

		struct {
			ASTList * statements;
		} block;

		struct {
			bool isConst;
			TypeKind typeKind;
			char * name;
			ASTNode * initializer;
		} declaration;

		struct {
			char * name;
			ASTNode * channel;
		} trackInit;

		struct {
			char * name;
			ASTNode * value;
		} assignment;

		struct {
			char * trackName;
			ASTNode * note;
			ASTNode * duration;
		} play;

		struct {
			char * trackName;
			ASTNode * duration;
		} rest;

		struct {
			ASTNode * block;
		} syncBlock;

		struct {
			CCKind kind;
			char * trackName;
			ASTNode * value;
		} ccStmt;

		struct {
			ASTNode * condition;
			ASTNode * thenBlock;
			ASTNode * elseBlock;
		} ifStmt;

		struct {
			bool hasType;
			TypeKind initTypeKind;
			char * initName;
			ASTNode * initValue;
			ASTNode * condition;
			ASTNode * step;
			ASTNode * body;
		} forStmt;

		struct {
			ASTNode * condition;
			ASTNode * body;
		} whileStmt;

		struct {
			OperatorKind op;
			ASTNode * left;
			ASTNode * right;
		} binary;

		struct {
			OperatorKind op;
			ASTNode * operand;
		} unary;

		struct {
			int value;
		} intLit;

		struct {
			float value;
		} floatLit;

		struct {
			char * name;
		} identifier;

		struct {
			ASTNode * inner;
		} paren;
	} data;
};

extern ASTNode * root;

ASTNode * newASTNode(NodeType type);
ASTList * appendASTList(ASTList * list, ASTNode * node);

/**
 * Node recursive super-duper-trambolik-destructors.
 */
void destroyASTNode(ASTNode * node);
void destroyASTList(ASTList * list);

const char * typeKindName(TypeKind kind);
const char * operatorKindName(OperatorKind op);
const char * ccKindName(CCKind kind);
const char * nodeTypeName(NodeType type);

#endif
