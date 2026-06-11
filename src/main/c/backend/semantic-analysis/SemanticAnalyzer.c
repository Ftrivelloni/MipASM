#include "SemanticAnalyzer.h"
#include <string.h>

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownSemanticAnalyzerModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: SemanticAnalyzer...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeSemanticAnalyzerModule() {
	_logger = createLogger("SemanticAnalyzer");
	return _shutdownSemanticAnalyzerModule;
}

/* PRIVATE TYPES */

typedef enum {
	EXPR_INVALID,
	EXPR_INT,
	EXPR_FLOAT,
	EXPR_BOOLEAN,
	EXPR_TRACK,
	EXPR_DURATION
} ExpressionType;

typedef struct Symbol {
	char * name;
	TypeKind type;
	bool isConst;
	struct Symbol * next;
} Symbol;

typedef struct Scope {
	Symbol * symbols;
	struct Scope * parent;
} Scope;

typedef struct {
	Scope * scope;
	unsigned int errorCount;
} AnalyzerContext;

/* PRIVATE FUNCTIONS */

static void _analyzeNode(AnalyzerContext * context, ASTNode * node);
static void _analyzeList(AnalyzerContext * context, ASTList * list);
static ExpressionType _analyzeExpression(AnalyzerContext * context, ASTNode * node);

static ExpressionType _typeKindToExpressionType(TypeKind type) {
	switch (type) {
		case TYPE_INT: return EXPR_INT;
		case TYPE_FLOAT: return EXPR_FLOAT;
		case TYPE_BOOLEAN: return EXPR_BOOLEAN;
		case TYPE_TRACK: return EXPR_TRACK;
		case TYPE_DURATION: return EXPR_DURATION;
		default: return EXPR_INVALID;
	}
}

static TypeKind _expressionTypeToTypeKind(ExpressionType type) {
	switch (type) {
		case EXPR_INT: return TYPE_INT;
		case EXPR_FLOAT: return TYPE_FLOAT;
		case EXPR_BOOLEAN: return TYPE_BOOLEAN;
		case EXPR_TRACK: return TYPE_TRACK;
		case EXPR_DURATION: return TYPE_DURATION;
		default: return TYPE_INT;
	}
}

static const char * _expressionTypeName(ExpressionType type) {
	if (type == EXPR_INVALID) {
		return "invalid";
	}
	return typeKindName(_expressionTypeToTypeKind(type));
}

static void _reportError(AnalyzerContext * context, const char * const format, ...) {
	context->errorCount++;
	va_list arguments;
	va_start(arguments, format);
	fprintf(stderr, ERROR_COLOR "[ERROR][SemanticAnalyzer] " DEFAULT_COLOR);
	vfprintf(stderr, format, arguments);
	fprintf(stderr, "\n");
	va_end(arguments);
}

static bool _isNumeric(ExpressionType type) {
	return type == EXPR_INT || type == EXPR_FLOAT || type == EXPR_DURATION;
}

static bool _isBooleanCompatible(ExpressionType type) {
	return type == EXPR_INT || type == EXPR_BOOLEAN;
}

static bool _isDurationCompatible(ExpressionType type) {
	return type == EXPR_INT || type == EXPR_FLOAT || type == EXPR_DURATION;
}

static bool _isAssignable(ExpressionType expected, ExpressionType actual) {
	if (expected == EXPR_INVALID || actual == EXPR_INVALID) {
		return false;
	}
	if (expected == actual) {
		return true;
	}
	switch (expected) {
		case EXPR_FLOAT:
			return actual == EXPR_INT;
		case EXPR_DURATION:
			return actual == EXPR_INT || actual == EXPR_FLOAT;
		case EXPR_BOOLEAN:
			return actual == EXPR_INT;
		default:
			return false;
	}
}

static Scope * _pushScope(AnalyzerContext * context) {
	Scope * scope = (Scope *) calloc(1, sizeof(Scope));
	scope->parent = context->scope;
	context->scope = scope;
	return scope;
}

static void _destroySymbols(Symbol * symbol) {
	while (symbol != NULL) {
		Symbol * next = symbol->next;
		free(symbol->name);
		free(symbol);
		symbol = next;
	}
}

static void _popScope(AnalyzerContext * context) {
	if (context->scope == NULL) {
		return;
	}
	Scope * parent = context->scope->parent;
	_destroySymbols(context->scope->symbols);
	free(context->scope);
	context->scope = parent;
}

static Symbol * _findInCurrentScope(AnalyzerContext * context, const char * name) {
	if (context->scope == NULL) {
		return NULL;
	}
	for (Symbol * symbol = context->scope->symbols; symbol != NULL; symbol = symbol->next) {
		if (strcmp(symbol->name, name) == 0) {
			return symbol;
		}
	}
	return NULL;
}

static Symbol * _findSymbol(AnalyzerContext * context, const char * name) {
	for (Scope * scope = context->scope; scope != NULL; scope = scope->parent) {
		for (Symbol * symbol = scope->symbols; symbol != NULL; symbol = symbol->next) {
			if (strcmp(symbol->name, name) == 0) {
				return symbol;
			}
		}
	}
	return NULL;
}

static bool _declareSymbol(AnalyzerContext * context, const char * name, TypeKind type, bool isConst) {
	if (_findInCurrentScope(context, name) != NULL) {
		_reportError(context, "Duplicate declaration of '%s'.", name);
		return false;
	}
	Symbol * symbol = (Symbol *) calloc(1, sizeof(Symbol));
	symbol->name = strdup(name);
	symbol->type = type;
	symbol->isConst = isConst;
	symbol->next = context->scope->symbols;
	context->scope->symbols = symbol;
	return true;
}

static void _requireTrack(AnalyzerContext * context, const char * name) {
	Symbol * symbol = _findSymbol(context, name);
	if (symbol == NULL) {
		_reportError(context, "Track '%s' is not declared.", name);
		return;
	}
	if (symbol->type != TYPE_TRACK) {
		_reportError(context, "'%s' is %s, not track.", name, typeKindName(symbol->type));
	}
}

static void _analyzeDeclaration(AnalyzerContext * context, ASTNode * node) {
	const char * name = node->data.declaration.name;
	ExpressionType expected = _typeKindToExpressionType(node->data.declaration.typeKind);
	if (node->data.declaration.initializer != NULL) {
		ExpressionType actual = _analyzeExpression(context, node->data.declaration.initializer);
		if (!_isAssignable(expected, actual)) {
			_reportError(context, "Cannot initialize '%s' as %s with %s.",
				name,
				_expressionTypeName(expected),
				_expressionTypeName(actual));
		}
	}
	_declareSymbol(context, name, node->data.declaration.typeKind, node->data.declaration.isConst);
}

static void _analyzeTrackInit(AnalyzerContext * context, ASTNode * node) {
	ExpressionType channelType = _analyzeExpression(context, node->data.trackInit.channel);
	if (!_isNumeric(channelType)) {
		_reportError(context, "Track '%s' channel must be numeric, got %s.",
			node->data.trackInit.name,
			_expressionTypeName(channelType));
	}
	_declareSymbol(context, node->data.trackInit.name, TYPE_TRACK, false);
}

static void _analyzeAssignment(AnalyzerContext * context, ASTNode * node) {
	Symbol * symbol = _findSymbol(context, node->data.assignment.name);
	if (symbol == NULL) {
		_reportError(context, "Assignment target '%s' is not declared.", node->data.assignment.name);
		_analyzeExpression(context, node->data.assignment.value);
		return;
	}
	if (symbol->isConst) {
		_reportError(context, "Cannot assign to const '%s'.", symbol->name);
	}
	ExpressionType expected = _typeKindToExpressionType(symbol->type);
	ExpressionType actual = _analyzeExpression(context, node->data.assignment.value);
	if (!_isAssignable(expected, actual)) {
		_reportError(context, "Cannot assign %s to '%s' of type %s.",
			_expressionTypeName(actual),
			symbol->name,
			_expressionTypeName(expected));
	}
}

static void _analyzeBlock(AnalyzerContext * context, ASTNode * node) {
	_pushScope(context);
	_analyzeList(context, node->data.block.statements);
	_popScope(context);
}

static void _analyzeFor(AnalyzerContext * context, ASTNode * node) {
	_pushScope(context);
	if (node->data.forStmt.hasType) {
		ExpressionType expected = _typeKindToExpressionType(node->data.forStmt.initTypeKind);
		ExpressionType actual = _analyzeExpression(context, node->data.forStmt.initValue);
		if (!_isAssignable(expected, actual)) {
			_reportError(context, "Cannot initialize for-loop variable '%s' as %s with %s.",
				node->data.forStmt.initName,
				_expressionTypeName(expected),
				_expressionTypeName(actual));
		}
		_declareSymbol(context, node->data.forStmt.initName, node->data.forStmt.initTypeKind, false);
	} else {
		ASTNode assignment = {0};
		assignment.nodeType = AST_ASSIGNMENT;
		assignment.data.assignment.name = node->data.forStmt.initName;
		assignment.data.assignment.value = node->data.forStmt.initValue;
		_analyzeAssignment(context, &assignment);
	}
	ExpressionType conditionType = _analyzeExpression(context, node->data.forStmt.condition);
	if (!_isBooleanCompatible(conditionType)) {
		_reportError(context, "For-loop condition must be boolean-compatible, got %s.",
			_expressionTypeName(conditionType));
	}
	_analyzeNode(context, node->data.forStmt.step);
	_analyzeNode(context, node->data.forStmt.body);
	_popScope(context);
}

static void _analyzeNode(AnalyzerContext * context, ASTNode * node) {
	if (node == NULL) {
		return;
	}
	switch (node->nodeType) {
		case AST_PROGRAM:
			_pushScope(context);
			_analyzeList(context, node->data.program.globalDecls);
			_analyzeNode(context, node->data.program.mainFunc);
			_popScope(context);
			break;
		case AST_INCLUDE:
			break;
		case AST_MAIN_FUNC:
			_analyzeNode(context, node->data.mainFunc.body);
			break;
		case AST_BLOCK:
			_analyzeBlock(context, node);
			break;
		case AST_DECLARATION:
			_analyzeDeclaration(context, node);
			break;
		case AST_TRACK_INIT:
			_analyzeTrackInit(context, node);
			break;
		case AST_ASSIGNMENT:
			_analyzeAssignment(context, node);
			break;
		case AST_PLAY:
			_requireTrack(context, node->data.play.trackName);
			if (!_isNumeric(_analyzeExpression(context, node->data.play.note))) {
				_reportError(context, "play note expression must be numeric.");
			}
			if (!_isDurationCompatible(_analyzeExpression(context, node->data.play.duration))) {
				_reportError(context, "play duration expression must be duration-compatible.");
			}
			break;
		case AST_REST:
			_requireTrack(context, node->data.rest.trackName);
			if (!_isDurationCompatible(_analyzeExpression(context, node->data.rest.duration))) {
				_reportError(context, "rest duration expression must be duration-compatible.");
			}
			break;
		case AST_SYNC_BLOCK:
			_analyzeNode(context, node->data.syncBlock.block);
			break;
		case AST_CC_STMT:
			_requireTrack(context, node->data.ccStmt.trackName);
			if (!_isNumeric(_analyzeExpression(context, node->data.ccStmt.value))) {
				_reportError(context, "%s value must be numeric.", ccKindName(node->data.ccStmt.kind));
			}
			break;
		case AST_TEMPO_STMT:
			if (!_isNumeric(_analyzeExpression(context, node->data.tempoStmt.bpm))) {
				_reportError(context, "set_tempo value must be numeric.");
			}
			break;
		case AST_TIME_SIGNATURE_STMT:
			if (_analyzeExpression(context, node->data.timeSignatureStmt.numerator) != EXPR_INT) {
				_reportError(context, "set_time_signature numerator must be int.");
			}
			if (_analyzeExpression(context, node->data.timeSignatureStmt.denominator) != EXPR_INT) {
				_reportError(context, "set_time_signature denominator must be int.");
			}
			break;
		case AST_INSTRUMENT_STMT:
			_requireTrack(context, node->data.instrumentStmt.trackName);
			if (!_isNumeric(_analyzeExpression(context, node->data.instrumentStmt.program))) {
				_reportError(context, "set_instrument program must be numeric.");
			}
			break;
		case AST_IF:
			if (!_isBooleanCompatible(_analyzeExpression(context, node->data.ifStmt.condition))) {
				_reportError(context, "If condition must be boolean-compatible.");
			}
			_analyzeNode(context, node->data.ifStmt.thenBlock);
			_analyzeNode(context, node->data.ifStmt.elseBlock);
			break;
		case AST_FOR:
			_analyzeFor(context, node);
			break;
		case AST_WHILE:
			if (!_isBooleanCompatible(_analyzeExpression(context, node->data.whileStmt.condition))) {
				_reportError(context, "While condition must be boolean-compatible.");
			}
			_analyzeNode(context, node->data.whileStmt.body);
			break;
		case AST_BINARY_OP:
		case AST_UNARY_OP:
		case AST_INT_LIT:
		case AST_FLOAT_LIT:
		case AST_IDENTIFIER:
		case AST_PAREN_EXPR:
			_analyzeExpression(context, node);
			break;
	}
}

static void _analyzeList(AnalyzerContext * context, ASTList * list) {
	for (ASTList * cell = list; cell != NULL; cell = cell->next) {
		_analyzeNode(context, cell->node);
	}
}

static ExpressionType _analyzeExpression(AnalyzerContext * context, ASTNode * node) {
	if (node == NULL) {
		return EXPR_INVALID;
	}
	switch (node->nodeType) {
		case AST_INT_LIT:
			return EXPR_INT;
		case AST_FLOAT_LIT:
			return EXPR_FLOAT;
		case AST_IDENTIFIER: {
			Symbol * symbol = _findSymbol(context, node->data.identifier.name);
			if (symbol == NULL) {
				_reportError(context, "Identifier '%s' is not declared.", node->data.identifier.name);
				return EXPR_INVALID;
			}
			return _typeKindToExpressionType(symbol->type);
		}
		case AST_PAREN_EXPR:
			return _analyzeExpression(context, node->data.paren.inner);
		case AST_UNARY_OP: {
			ExpressionType operandType = _analyzeExpression(context, node->data.unary.operand);
			if (node->data.unary.op == OP_NOT) {
				if (!_isBooleanCompatible(operandType)) {
					_reportError(context, "Operator ! requires a boolean-compatible operand, got %s.",
						_expressionTypeName(operandType));
				}
				return EXPR_BOOLEAN;
			}
			return EXPR_INVALID;
		}
		case AST_BINARY_OP: {
			ExpressionType leftType = _analyzeExpression(context, node->data.binary.left);
			ExpressionType rightType = _analyzeExpression(context, node->data.binary.right);
			switch (node->data.binary.op) {
				case OP_ADD:
				case OP_SUB:
				case OP_MUL:
				case OP_DIV:
					if (!_isNumeric(leftType) || !_isNumeric(rightType)) {
						_reportError(context, "Operator %s requires numeric operands, got %s and %s.",
							operatorKindName(node->data.binary.op),
							_expressionTypeName(leftType),
							_expressionTypeName(rightType));
						return EXPR_INVALID;
					}
					if (leftType == EXPR_FLOAT || rightType == EXPR_FLOAT) {
						return EXPR_FLOAT;
					}
					if (leftType == EXPR_DURATION || rightType == EXPR_DURATION) {
						return EXPR_DURATION;
					}
					return EXPR_INT;
				case OP_EQ:
				case OP_NEQ:
					if (!_isAssignable(leftType, rightType) && !_isAssignable(rightType, leftType)) {
						_reportError(context, "Operator %s cannot compare %s and %s.",
							operatorKindName(node->data.binary.op),
							_expressionTypeName(leftType),
							_expressionTypeName(rightType));
					}
					return EXPR_BOOLEAN;
				case OP_LT:
				case OP_GT:
				case OP_LEQ:
				case OP_GEQ:
					if (!_isNumeric(leftType) || !_isNumeric(rightType)) {
						_reportError(context, "Operator %s requires numeric operands, got %s and %s.",
							operatorKindName(node->data.binary.op),
							_expressionTypeName(leftType),
							_expressionTypeName(rightType));
					}
					return EXPR_BOOLEAN;
				case OP_AND:
				case OP_OR:
					if (!_isBooleanCompatible(leftType) || !_isBooleanCompatible(rightType)) {
						_reportError(context, "Operator %s requires boolean-compatible operands, got %s and %s.",
							operatorKindName(node->data.binary.op),
							_expressionTypeName(leftType),
							_expressionTypeName(rightType));
					}
					return EXPR_BOOLEAN;
				default:
					return EXPR_INVALID;
			}
		}
		default:
			_reportError(context, "Node %s cannot be used as an expression.", nodeTypeName(node->nodeType));
			return EXPR_INVALID;
	}
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeSemanticAnalysis(CompilerState * compilerState) {
	logDebugging(_logger, "Analyzing semantics...");
	AnalyzerContext context = {
		.scope = NULL,
		.errorCount = 0
	};
	ASTNode * tree = (ASTNode *) compilerState->abstractSyntaxtTree;
	if (tree == NULL) {
		_reportError(&context, "Cannot analyze an empty AST.");
	} else {
		_analyzeNode(&context, tree);
	}
	while (context.scope != NULL) {
		_popScope(&context);
	}
	if (context.errorCount == 0) {
		logDebugging(_logger, "Semantic analysis accepted the program.");
		return SUCCEEDED;
	}
	logError(_logger, "Semantic analysis rejected the program with %u error(s).", context.errorCount);
	return FAILED;
}
