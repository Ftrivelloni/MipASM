#include "Generator.h"

static Logger * _logger = NULL;

void _shutdownGeneratorModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Generator...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeGeneratorModule() {
	_logger = createLogger("Generator");
	return _shutdownGeneratorModule;
}

static void _printIndent(unsigned int level) {
	for (unsigned int i = 0; i < level; ++i) {
		fputs("  ", stdout);
	}
}

static void _printNode(ASTNode * node, unsigned int level);

static void _printList(const char * label, ASTList * list, unsigned int level) {
	_printIndent(level);
	if (list == NULL) {
		printf("%s: []\n", label);
		return;
	}
	printf("%s:\n", label);
	for (ASTList * cell = list; cell != NULL; cell = cell->next) {
		_printNode(cell->node, level + 1);
	}
}

static void _printNode(ASTNode * node, unsigned int level) {
	if (node == NULL) {
		_printIndent(level);
		printf("(null)\n");
		return;
	}
	_printIndent(level);
	printf("%s", nodeTypeName(node->nodeType));
	switch (node->nodeType) {
		case AST_PROGRAM:
			printf("\n");
			_printList("includes", node->data.program.includes, level + 1);
			_printList("globalDecls", node->data.program.globalDecls, level + 1);
			_printIndent(level + 1);
			printf("mainFunc:\n");
			_printNode(node->data.program.mainFunc, level + 2);
			break;
		case AST_INCLUDE:
			printf(" \"%s\"\n", node->data.include.path ? node->data.include.path : "");
			break;
		case AST_MAIN_FUNC:
			printf("\n");
			_printNode(node->data.mainFunc.body, level + 1);
			break;
		case AST_BLOCK:
			printf("\n");
			_printList("statements", node->data.block.statements, level + 1);
			break;
		case AST_DECLARATION:
			printf(" %s%s %s\n",
				node->data.declaration.isConst ? "const " : "",
				typeKindName(node->data.declaration.typeKind),
				node->data.declaration.name ? node->data.declaration.name : "?");
			if (node->data.declaration.initializer != NULL) {
				_printIndent(level + 1);
				printf("init:\n");
				_printNode(node->data.declaration.initializer, level + 2);
			}
			break;
		case AST_TRACK_INIT:
			printf(" %s\n", node->data.trackInit.name ? node->data.trackInit.name : "?");
			_printIndent(level + 1);
			printf("channel:\n");
			_printNode(node->data.trackInit.channel, level + 2);
			break;
		case AST_ASSIGNMENT:
			printf(" %s\n", node->data.assignment.name ? node->data.assignment.name : "?");
			_printIndent(level + 1);
			printf("value:\n");
			_printNode(node->data.assignment.value, level + 2);
			break;
		case AST_PLAY:
			printf(" %s\n", node->data.play.trackName ? node->data.play.trackName : "?");
			_printIndent(level + 1);
			printf("note:\n");
			_printNode(node->data.play.note, level + 2);
			_printIndent(level + 1);
			printf("duration:\n");
			_printNode(node->data.play.duration, level + 2);
			break;
		case AST_REST:
			printf(" %s\n", node->data.rest.trackName ? node->data.rest.trackName : "?");
			_printIndent(level + 1);
			printf("duration:\n");
			_printNode(node->data.rest.duration, level + 2);
			break;
		case AST_SYNC_BLOCK:
			printf("\n");
			_printNode(node->data.syncBlock.block, level + 1);
			break;
		case AST_CC_STMT:
			printf(" %s(%s)\n",
				ccKindName(node->data.ccStmt.kind),
				node->data.ccStmt.trackName ? node->data.ccStmt.trackName : "?");
			_printIndent(level + 1);
			printf("value:\n");
			_printNode(node->data.ccStmt.value, level + 2);
			break;
		case AST_IF:
			printf("\n");
			_printIndent(level + 1);
			printf("condition:\n");
			_printNode(node->data.ifStmt.condition, level + 2);
			_printIndent(level + 1);
			printf("then:\n");
			_printNode(node->data.ifStmt.thenBlock, level + 2);
			if (node->data.ifStmt.elseBlock != NULL) {
				_printIndent(level + 1);
				printf("else:\n");
				_printNode(node->data.ifStmt.elseBlock, level + 2);
			}
			break;
		case AST_FOR:
			printf(" %s%s %s\n",
				node->data.forStmt.hasType ? typeKindName(node->data.forStmt.initTypeKind) : "",
				node->data.forStmt.hasType ? " " : "",
				node->data.forStmt.initName ? node->data.forStmt.initName : "?");
			_printIndent(level + 1);
			printf("init:\n");
			_printNode(node->data.forStmt.initValue, level + 2);
			_printIndent(level + 1);
			printf("condition:\n");
			_printNode(node->data.forStmt.condition, level + 2);
			_printIndent(level + 1);
			printf("step:\n");
			_printNode(node->data.forStmt.step, level + 2);
			_printIndent(level + 1);
			printf("body:\n");
			_printNode(node->data.forStmt.body, level + 2);
			break;
		case AST_WHILE:
			printf("\n");
			_printIndent(level + 1);
			printf("condition:\n");
			_printNode(node->data.whileStmt.condition, level + 2);
			_printIndent(level + 1);
			printf("body:\n");
			_printNode(node->data.whileStmt.body, level + 2);
			break;
		case AST_BINARY_OP:
			printf(" %s\n", operatorKindName(node->data.binary.op));
			_printNode(node->data.binary.left, level + 1);
			_printNode(node->data.binary.right, level + 1);
			break;
		case AST_UNARY_OP:
			printf(" %s\n", operatorKindName(node->data.unary.op));
			_printNode(node->data.unary.operand, level + 1);
			break;
		case AST_INT_LIT:
			printf(" %d\n", node->data.intLit.value);
			break;
		case AST_FLOAT_LIT:
			printf(" %f\n", (double) node->data.floatLit.value);
			break;
		case AST_IDENTIFIER:
			printf(" %s\n", node->data.identifier.name ? node->data.identifier.name : "?");
			break;
		case AST_PAREN_EXPR:
			printf("\n");
			_printNode(node->data.paren.inner, level + 1);
			break;
		default:
			printf(" (unknown)\n");
			break;
	}
}

void executeGenerator(CompilerState * compilerState) {
	logDebugging(_logger, "Generating AST dump...");
	ASTNode * tree = (ASTNode *) compilerState->abstractSyntaxtTree;
	if (tree == NULL) {
		printf("(empty AST)\n");
	} else {
		_printNode(tree, 0);
	}
	fflush(stdout);
	logDebugging(_logger, "Generation is done.");
}
