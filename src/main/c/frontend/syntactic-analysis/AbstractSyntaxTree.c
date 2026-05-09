#include "AbstractSyntaxTree.h"

/* MODULE INTERNAL STATE */

ASTNode * root = NULL;

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownAbstractSyntaxTreeModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: AbstractSyntaxTree...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

/* PUBLIC FUNCTIONS */

ModuleDestructor initializeAbstractSyntaxTreeModule() {
	_logger = createLogger("AbstractSyntaxTree");
	return _shutdownAbstractSyntaxTreeModule;
}

ASTNode * newASTNode(NodeType type) {
	ASTNode * node = (ASTNode *) calloc(1, sizeof(ASTNode));
	node->nodeType = type;
	return node;
}

ASTList * appendASTList(ASTList * list, ASTNode * node) {
	ASTList * cell = (ASTList *) calloc(1, sizeof(ASTList));
	cell->node = node;
	cell->next = NULL;
	if (list == NULL) {
		return cell;
	}
	ASTList * tail = list;
	while (tail->next != NULL) {
		tail = tail->next;
	}
	tail->next = cell;
	return list;
}

void destroyASTList(ASTList * list) {
	while (list != NULL) {
		ASTList * next = list->next;
		destroyASTNode(list->node);
		free(list);
		list = next;
	}
}

void destroyASTNode(ASTNode * node) {
	if (node == NULL) {
		return;
	}
	switch (node->nodeType) {
		case AST_PROGRAM:
			destroyASTList(node->data.program.includes);
			destroyASTList(node->data.program.globalDecls);
			destroyASTNode(node->data.program.mainFunc);
			break;
		case AST_INCLUDE:
			free(node->data.include.path);
			break;
		case AST_MAIN_FUNC:
			destroyASTNode(node->data.mainFunc.body);
			break;
		case AST_BLOCK:
			destroyASTList(node->data.block.statements);
			break;
		case AST_DECLARATION:
			free(node->data.declaration.name);
			destroyASTNode(node->data.declaration.initializer);
			break;
		case AST_TRACK_INIT:
			free(node->data.trackInit.name);
			destroyASTNode(node->data.trackInit.channel);
			break;
		case AST_ASSIGNMENT:
			free(node->data.assignment.name);
			destroyASTNode(node->data.assignment.value);
			break;
		case AST_PLAY:
			free(node->data.play.trackName);
			destroyASTNode(node->data.play.note);
			destroyASTNode(node->data.play.duration);
			break;
		case AST_REST:
			free(node->data.rest.trackName);
			destroyASTNode(node->data.rest.duration);
			break;
		case AST_SYNC_BLOCK:
			destroyASTNode(node->data.syncBlock.block);
			break;
		case AST_CC_STMT:
			free(node->data.ccStmt.trackName);
			destroyASTNode(node->data.ccStmt.value);
			break;
		case AST_IF:
			destroyASTNode(node->data.ifStmt.condition);
			destroyASTNode(node->data.ifStmt.thenBlock);
			destroyASTNode(node->data.ifStmt.elseBlock);
			break;
		case AST_FOR:
			free(node->data.forStmt.initName);
			destroyASTNode(node->data.forStmt.initValue);
			destroyASTNode(node->data.forStmt.condition);
			destroyASTNode(node->data.forStmt.step);
			destroyASTNode(node->data.forStmt.body);
			break;
		case AST_WHILE:
			destroyASTNode(node->data.whileStmt.condition);
			destroyASTNode(node->data.whileStmt.body);
			break;
		case AST_BINARY_OP:
			destroyASTNode(node->data.binary.left);
			destroyASTNode(node->data.binary.right);
			break;
		case AST_UNARY_OP:
			destroyASTNode(node->data.unary.operand);
			break;
		case AST_IDENTIFIER:
			free(node->data.identifier.name);
			break;
		case AST_PAREN_EXPR:
			destroyASTNode(node->data.paren.inner);
			break;
		case AST_INT_LIT:
		case AST_FLOAT_LIT:
			break;
	}
	free(node);
}

const char * typeKindName(TypeKind kind) {
	switch (kind) {
		case TYPE_INT: return "int";
		case TYPE_FLOAT: return "float";
		case TYPE_BOOLEAN: return "boolean";
		case TYPE_TRACK: return "track";
		case TYPE_DURATION: return "duration";
		default: return "?";
	}
}

const char * operatorKindName(OperatorKind op) {
	switch (op) {
		case OP_ADD: return "+";
		case OP_SUB: return "-";
		case OP_MUL: return "*";
		case OP_DIV: return "/";
		case OP_EQ:  return "==";
		case OP_NEQ: return "!=";
		case OP_LT:  return "<";
		case OP_GT:  return ">";
		case OP_LEQ: return "<=";
		case OP_GEQ: return ">=";
		case OP_AND: return "&&";
		case OP_OR:  return "||";
		case OP_NOT: return "!";
		default: return "?";
	}
}

const char * ccKindName(CCKind kind) {
	switch (kind) {
		case CC_VOLUME: return "set_volume";
		case CC_PAN:    return "set_pan";
		case CC_ATTACK: return "set_attack";
		default: return "?";
	}
}

const char * nodeTypeName(NodeType type) {
	switch (type) {
		case AST_PROGRAM:     return "Program";
		case AST_INCLUDE:     return "Include";
		case AST_MAIN_FUNC:   return "MainFunc";
		case AST_BLOCK:       return "Block";
		case AST_DECLARATION: return "Declaration";
		case AST_TRACK_INIT:  return "TrackInit";
		case AST_ASSIGNMENT:  return "Assignment";
		case AST_PLAY:        return "Play";
		case AST_REST:        return "Rest";
		case AST_SYNC_BLOCK:  return "SyncBlock";
		case AST_CC_STMT:     return "CC";
		case AST_IF:          return "If";
		case AST_FOR:         return "For";
		case AST_WHILE:       return "While";
		case AST_BINARY_OP:   return "BinaryOp";
		case AST_UNARY_OP:    return "UnaryOp";
		case AST_INT_LIT:     return "IntLit";
		case AST_FLOAT_LIT:   return "FloatLit";
		case AST_IDENTIFIER:  return "Identifier";
		case AST_PAREN_EXPR:  return "Paren";
		default: return "?";
	}
}
