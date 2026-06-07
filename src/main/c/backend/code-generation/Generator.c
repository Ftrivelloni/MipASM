#include "Generator.h"
#include <string.h>

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;
static const unsigned int MAX_LOOP_ITERATIONS = 100000;

/** Shutdown module's internal state. */
void _shutdownGeneratorModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Generator...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

/* PUBLIC FUNCTIONS */

ModuleDestructor initializeGeneratorModule() {
	_logger = createLogger("Generator");
	return _shutdownGeneratorModule;
}

/* PRIVATE FUNCTIONS */

static void _printIndent(unsigned int level) {
	for (unsigned int i = 0; i < level; ++i) {
		fputs("  ", stdout);
	}
}

static void _printNode(ASTNode * node, unsigned int level);

/**
 * Generates the output for a linked list of AST nodes.
 * This is for printing the AST / same as given
 */
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

/* INTERPRETER TYPES */

typedef struct TrackValue {
	const char * name;
	int channel;
	struct TrackValue * next;
} TrackValue;

typedef struct {
	TypeKind type;
	bool initialized;
	union {
		int intValue;
		double floatValue;
		bool boolValue;
		double durationValue;
		TrackValue * trackValue;
	} data;
} RuntimeValue;

typedef struct RuntimeSymbol {
	char * name;
	bool isConst;
	RuntimeValue value;
	struct RuntimeSymbol * next;
} RuntimeSymbol;

typedef struct RuntimeScope {
	RuntimeSymbol * symbols;
	struct RuntimeScope * parent;
} RuntimeScope;

typedef struct EventLine {
	char * text;
	struct EventLine * next;
} EventLine;

typedef struct {
	RuntimeScope * scope;
	TrackValue * tracks;
	EventLine * eventsHead;
	EventLine * eventsTail;
	double currentTime;
	unsigned int errorCount;
} InterpreterContext;

/* INTERPRETER DECLARATIONS */

static void _executeNode(InterpreterContext * context, ASTNode * node);
static void _executeList(InterpreterContext * context, ASTList * list);
static RuntimeValue _evaluateExpression(InterpreterContext * context, ASTNode * node);

/* INTERPRETER HELPERS */

static void _reportGenerationError(InterpreterContext * context, const char * const format, ...) {
	context->errorCount++;
	va_list arguments;
	va_start(arguments, format);
	fprintf(stderr, ERROR_COLOR "[ERROR][Generator] " DEFAULT_COLOR);
	vfprintf(stderr, format, arguments);
	fprintf(stderr, "\n");
	va_end(arguments);
}

static RuntimeValue _uninitializedValue(TypeKind type) {
	RuntimeValue value = {0};
	value.type = type;
	value.initialized = false;
	return value;
}

static RuntimeValue _intValue(int number) {
	RuntimeValue value = _uninitializedValue(TYPE_INT);
	value.initialized = true;
	value.data.intValue = number;
	return value;
}

static RuntimeValue _floatValue(double number) {
	RuntimeValue value = _uninitializedValue(TYPE_FLOAT);
	value.initialized = true;
	value.data.floatValue = number;
	return value;
}

static RuntimeValue _booleanValue(bool boolean) {
	RuntimeValue value = _uninitializedValue(TYPE_BOOLEAN);
	value.initialized = true;
	value.data.boolValue = boolean;
	return value;
}

static RuntimeValue _durationValue(double duration) {
	RuntimeValue value = _uninitializedValue(TYPE_DURATION);
	value.initialized = true;
	value.data.durationValue = duration;
	return value;
}

static RuntimeValue _trackValue(TrackValue * track) {
	RuntimeValue value = _uninitializedValue(TYPE_TRACK);
	value.initialized = true;
	value.data.trackValue = track;
	return value;
}

static bool _isNumericValue(RuntimeValue value) {
	return value.type == TYPE_INT || value.type == TYPE_FLOAT || value.type == TYPE_DURATION;
}

static double _asNumber(RuntimeValue value) {
	switch (value.type) {
		case TYPE_INT: return (double) value.data.intValue;
		case TYPE_FLOAT: return value.data.floatValue;
		case TYPE_BOOLEAN: return value.data.boolValue ? 1.0 : 0.0;
		case TYPE_DURATION: return value.data.durationValue;
		default: return 0.0;
	}
}

static bool _asBoolean(RuntimeValue value) {
	switch (value.type) {
		case TYPE_BOOLEAN: return value.data.boolValue;
		case TYPE_INT: return value.data.intValue != 0;
		case TYPE_FLOAT: return value.data.floatValue != 0.0;
		case TYPE_DURATION: return value.data.durationValue != 0.0;
		default: return false;
	}
}

static bool _coerceValue(RuntimeValue source, TypeKind targetType, RuntimeValue * destination) {
	if (!source.initialized) {
		*destination = _uninitializedValue(targetType);
		return false;
	}
	if (source.type == targetType) {
		*destination = source;
		return true;
	}
	switch (targetType) {
		case TYPE_FLOAT:
			if (source.type == TYPE_INT) {
				*destination = _floatValue((double) source.data.intValue);
				return true;
			}
			break;
		case TYPE_DURATION:
			if (source.type == TYPE_INT || source.type == TYPE_FLOAT) {
				*destination = _durationValue(_asNumber(source));
				return true;
			}
			break;
		case TYPE_BOOLEAN:
			if (source.type == TYPE_INT) {
				*destination = _booleanValue(source.data.intValue != 0);
				return true;
			}
			break;
		default:
			break;
	}
	return false;
}

static RuntimeScope * _pushRuntimeScope(InterpreterContext * context) {
	RuntimeScope * scope = (RuntimeScope *) calloc(1, sizeof(RuntimeScope));
	scope->parent = context->scope;
	context->scope = scope;
	return scope;
}

static void _destroyRuntimeSymbols(RuntimeSymbol * symbol) {
	while (symbol != NULL) {
		RuntimeSymbol * next = symbol->next;
		free(symbol->name);
		free(symbol);
		symbol = next;
	}
}

static void _popRuntimeScope(InterpreterContext * context) {
	if (context->scope == NULL) {
		return;
	}
	RuntimeScope * parent = context->scope->parent;
	_destroyRuntimeSymbols(context->scope->symbols);
	free(context->scope);
	context->scope = parent;
}

static RuntimeSymbol * _findRuntimeSymbol(InterpreterContext * context, const char * name) {
	for (RuntimeScope * scope = context->scope; scope != NULL; scope = scope->parent) {
		for (RuntimeSymbol * symbol = scope->symbols; symbol != NULL; symbol = symbol->next) {
			if (strcmp(symbol->name, name) == 0) {
				return symbol;
			}
		}
	}
	return NULL;
}

static RuntimeSymbol * _declareRuntimeSymbol(InterpreterContext * context, const char * name, bool isConst, RuntimeValue value) {
	RuntimeSymbol * symbol = (RuntimeSymbol *) calloc(1, sizeof(RuntimeSymbol));
	symbol->name = strdup(name);
	symbol->isConst = isConst;
	symbol->value = value;
	symbol->next = context->scope->symbols;
	context->scope->symbols = symbol;
	return symbol;
}

static TrackValue * _findTrack(InterpreterContext * context, const char * name) {
	RuntimeSymbol * symbol = _findRuntimeSymbol(context, name);
	if (symbol == NULL || symbol->value.type != TYPE_TRACK || !symbol->value.initialized) {
		_reportGenerationError(context, "Track '%s' cannot be evaluated.", name);
		return NULL;
	}
	return symbol->value.data.trackValue;
}

static void _appendEventLine(InterpreterContext * context, const char * const format, ...) {
	va_list arguments;
	va_start(arguments, format);
	int length = vsnprintf(NULL, 0, format, arguments);
	va_end(arguments);
	if (length < 0) {
		_reportGenerationError(context, "Cannot format generated event.");
		return;
	}
	char * text = (char *) calloc((size_t) length + 1, sizeof(char));
	va_start(arguments, format);
	vsnprintf(text, (size_t) length + 1, format, arguments);
	va_end(arguments);
	EventLine * line = (EventLine *) calloc(1, sizeof(EventLine));
	line->text = text;
	if (context->eventsTail == NULL) {
		context->eventsHead = line;
		context->eventsTail = line;
	} else {
		context->eventsTail->next = line;
		context->eventsTail = line;
	}
}

static void _destroyEvents(EventLine * line) {
	while (line != NULL) {
		EventLine * next = line->next;
		free(line->text);
		free(line);
		line = next;
	}
}

static void _destroyTracks(TrackValue * track) {
	while (track != NULL) {
		TrackValue * next = track->next;
		free(track);
		track = next;
	}
}

static void _printEvents(InterpreterContext * context) {
	printf("Events:\n");
	for (EventLine * line = context->eventsHead; line != NULL; line = line->next) {
		printf("%s\n", line->text);
	}
}

static RuntimeValue _requireInitialized(InterpreterContext * context, const char * label, RuntimeValue value) {
	if (!value.initialized) {
		_reportGenerationError(context, "%s is uninitialized.", label);
	}
	return value;
}

static RuntimeValue _evaluateBinary(InterpreterContext * context, ASTNode * node) {
	RuntimeValue left = _requireInitialized(context, "Left operand", _evaluateExpression(context, node->data.binary.left));
	RuntimeValue right = _requireInitialized(context, "Right operand", _evaluateExpression(context, node->data.binary.right));
	if (!left.initialized || !right.initialized) {
		return _uninitializedValue(TYPE_INT);
	}
	switch (node->data.binary.op) {
		case OP_ADD:
		case OP_SUB:
		case OP_MUL:
		case OP_DIV: {
			if (!_isNumericValue(left) || !_isNumericValue(right)) {
				_reportGenerationError(context, "Operator %s requires numeric values.", operatorKindName(node->data.binary.op));
				return _uninitializedValue(TYPE_INT);
			}
			double l = _asNumber(left);
			double r = _asNumber(right);
			double result = 0.0;
			switch (node->data.binary.op) {
				case OP_ADD: result = l + r; break;
				case OP_SUB: result = l - r; break;
				case OP_MUL: result = l * r; break;
				case OP_DIV: result = l / r; break;
				default: break;
			}
			if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
				return _floatValue(result);
			}
			if (left.type == TYPE_DURATION || right.type == TYPE_DURATION) {
				return _durationValue(result);
			}
			return _intValue((int) result);
		}
		case OP_EQ:
			return _booleanValue(_asNumber(left) == _asNumber(right));
		case OP_NEQ:
			return _booleanValue(_asNumber(left) != _asNumber(right));
		case OP_LT:
			return _booleanValue(_asNumber(left) < _asNumber(right));
		case OP_GT:
			return _booleanValue(_asNumber(left) > _asNumber(right));
		case OP_LEQ:
			return _booleanValue(_asNumber(left) <= _asNumber(right));
		case OP_GEQ:
			return _booleanValue(_asNumber(left) >= _asNumber(right));
		case OP_AND:
			return _booleanValue(_asBoolean(left) && _asBoolean(right));
		case OP_OR:
			return _booleanValue(_asBoolean(left) || _asBoolean(right));
		default:
			return _uninitializedValue(TYPE_INT);
	}
}

static RuntimeValue _evaluateExpression(InterpreterContext * context, ASTNode * node) {
	if (node == NULL) {
		_reportGenerationError(context, "Missing expression.");
		return _uninitializedValue(TYPE_INT);
	}
	switch (node->nodeType) {
		case AST_INT_LIT:
			return _intValue(node->data.intLit.value);
		case AST_FLOAT_LIT:
			return _floatValue((double) node->data.floatLit.value);
		case AST_IDENTIFIER: {
			RuntimeSymbol * symbol = _findRuntimeSymbol(context, node->data.identifier.name);
			if (symbol == NULL) {
				_reportGenerationError(context, "Identifier '%s' cannot be evaluated.", node->data.identifier.name);
				return _uninitializedValue(TYPE_INT);
			}
			if (!symbol->value.initialized) {
				_reportGenerationError(context, "Identifier '%s' is uninitialized.", symbol->name);
			}
			return symbol->value;
		}
		case AST_PAREN_EXPR:
			return _evaluateExpression(context, node->data.paren.inner);
		case AST_UNARY_OP: {
			RuntimeValue operand = _requireInitialized(context, "Unary operand", _evaluateExpression(context, node->data.unary.operand));
			if (!operand.initialized) {
				return _uninitializedValue(TYPE_BOOLEAN);
			}
			if (node->data.unary.op == OP_NOT) {
				return _booleanValue(!_asBoolean(operand));
			}
			_reportGenerationError(context, "Unsupported unary operator %s.", operatorKindName(node->data.unary.op));
			return _uninitializedValue(TYPE_BOOLEAN);
		}
		case AST_BINARY_OP:
			return _evaluateBinary(context, node);
		default:
			_reportGenerationError(context, "Node %s cannot be evaluated as an expression.", nodeTypeName(node->nodeType));
			return _uninitializedValue(TYPE_INT);
	}
}

static bool _assignRuntimeSymbol(InterpreterContext * context, const char * name, RuntimeValue value) {
	RuntimeSymbol * symbol = _findRuntimeSymbol(context, name);
	if (symbol == NULL) {
		_reportGenerationError(context, "Assignment target '%s' cannot be evaluated.", name);
		return false;
	}
	if (symbol->isConst) {
		_reportGenerationError(context, "Cannot assign to const '%s'.", name);
		return false;
	}
	RuntimeValue coerced = {0};
	if (!_coerceValue(value, symbol->value.type, &coerced)) {
		_reportGenerationError(context, "Cannot assign value to '%s' as %s.", name, typeKindName(symbol->value.type));
		return false;
	}
	symbol->value = coerced;
	return true;
}

static bool _conditionIsTrue(InterpreterContext * context, ASTNode * expression, const char * label) {
	RuntimeValue value = _requireInitialized(context, label, _evaluateExpression(context, expression));
	if (!value.initialized) {
		return false;
	}
	return _asBoolean(value);
}

static void _executeBlock(InterpreterContext * context, ASTNode * node) {
	_pushRuntimeScope(context);
	_executeList(context, node->data.block.statements);
	_popRuntimeScope(context);
}

static void _executeDeclaration(InterpreterContext * context, ASTNode * node) {
	RuntimeValue value = _uninitializedValue(node->data.declaration.typeKind);
	if (node->data.declaration.initializer != NULL) {
		RuntimeValue initializer = _evaluateExpression(context, node->data.declaration.initializer);
		if (!_coerceValue(initializer, node->data.declaration.typeKind, &value)) {
			_reportGenerationError(context, "Cannot initialize '%s' as %s.", node->data.declaration.name, typeKindName(node->data.declaration.typeKind));
			value = _uninitializedValue(node->data.declaration.typeKind);
		}
	}
	_declareRuntimeSymbol(context, node->data.declaration.name, node->data.declaration.isConst, value);
}

static void _executeTrackInit(InterpreterContext * context, ASTNode * node) {
	RuntimeValue channel = _requireInitialized(context, "Track channel", _evaluateExpression(context, node->data.trackInit.channel));
	if (!channel.initialized || !_isNumericValue(channel)) {
		_reportGenerationError(context, "Track '%s' channel cannot be evaluated.", node->data.trackInit.name);
		return;
	}
	TrackValue * track = (TrackValue *) calloc(1, sizeof(TrackValue));
	track->name = node->data.trackInit.name;
	track->channel = (int) _asNumber(channel);
	track->next = context->tracks;
	context->tracks = track;
	_declareRuntimeSymbol(context, node->data.trackInit.name, false, _trackValue(track));
	_appendEventLine(context, "track %s channel=%d", track->name, track->channel);
}

static void _executePlay(InterpreterContext * context, ASTNode * node) {
	TrackValue * track = _findTrack(context, node->data.play.trackName);
	RuntimeValue note = _requireInitialized(context, "play note", _evaluateExpression(context, node->data.play.note));
	RuntimeValue duration = _requireInitialized(context, "play duration", _evaluateExpression(context, node->data.play.duration));
	if (track == NULL || !note.initialized || !duration.initialized || !_isNumericValue(note) || !_isNumericValue(duration)) {
		_reportGenerationError(context, "play arguments cannot be evaluated.");
		return;
	}
	double durationNumber = _asNumber(duration);
	_appendEventLine(context, "t=%.3f play %s note=%d duration=%.3f",
		context->currentTime,
		track->name,
		(int) _asNumber(note),
		durationNumber);
	context->currentTime += durationNumber;
}

static void _executeRest(InterpreterContext * context, ASTNode * node) {
	TrackValue * track = _findTrack(context, node->data.rest.trackName);
	RuntimeValue duration = _requireInitialized(context, "rest duration", _evaluateExpression(context, node->data.rest.duration));
	if (track == NULL || !duration.initialized || !_isNumericValue(duration)) {
		_reportGenerationError(context, "rest arguments cannot be evaluated.");
		return;
	}
	double durationNumber = _asNumber(duration);
	_appendEventLine(context, "t=%.3f rest %s duration=%.3f",
		context->currentTime,
		track->name,
		durationNumber);
	context->currentTime += durationNumber;
}

static void _executeCC(InterpreterContext * context, ASTNode * node) {
	TrackValue * track = _findTrack(context, node->data.ccStmt.trackName);
	RuntimeValue value = _requireInitialized(context, "cc value", _evaluateExpression(context, node->data.ccStmt.value));
	if (track == NULL || !value.initialized || !_isNumericValue(value)) {
		_reportGenerationError(context, "%s arguments cannot be evaluated.", ccKindName(node->data.ccStmt.kind));
		return;
	}
	const char * control = "volume";
	if (node->data.ccStmt.kind == CC_PAN) {
		control = "pan";
	} else if (node->data.ccStmt.kind == CC_ATTACK) {
		control = "attack";
	}
	if (value.type == TYPE_INT) {
		_appendEventLine(context, "t=%.3f cc %s %s=%d",
			context->currentTime,
			track->name,
			control,
			value.data.intValue);
	} else {
		_appendEventLine(context, "t=%.3f cc %s %s=%.3f",
			context->currentTime,
			track->name,
			control,
			_asNumber(value));
	}
}

static void _executeFor(InterpreterContext * context, ASTNode * node) {
	_pushRuntimeScope(context);
	if (node->data.forStmt.hasType) {
		RuntimeValue initializer = _evaluateExpression(context, node->data.forStmt.initValue);
		RuntimeValue value = {0};
		if (!_coerceValue(initializer, node->data.forStmt.initTypeKind, &value)) {
			_reportGenerationError(context, "Cannot initialize for-loop variable '%s'.", node->data.forStmt.initName);
			value = _uninitializedValue(node->data.forStmt.initTypeKind);
		}
		_declareRuntimeSymbol(context, node->data.forStmt.initName, false, value);
	} else {
		_assignRuntimeSymbol(context, node->data.forStmt.initName, _evaluateExpression(context, node->data.forStmt.initValue));
	}
	unsigned int iterations = 0;
	while (_conditionIsTrue(context, node->data.forStmt.condition, "for condition")) {
		if (iterations++ >= MAX_LOOP_ITERATIONS) {
			_reportGenerationError(context, "For-loop exceeded iteration limit %u.", MAX_LOOP_ITERATIONS);
			break;
		}
		_executeNode(context, node->data.forStmt.body);
		_executeNode(context, node->data.forStmt.step);
		if (context->errorCount != 0) {
			break;
		}
	}
	_popRuntimeScope(context);
}

static void _executeWhile(InterpreterContext * context, ASTNode * node) {
	unsigned int iterations = 0;
	while (_conditionIsTrue(context, node->data.whileStmt.condition, "while condition")) {
		if (iterations++ >= MAX_LOOP_ITERATIONS) {
			_reportGenerationError(context, "While-loop exceeded iteration limit %u.", MAX_LOOP_ITERATIONS);
			break;
		}
		_executeNode(context, node->data.whileStmt.body);
		if (context->errorCount != 0) {
			break;
		}
	}
}

static void _executeSync(InterpreterContext * context, ASTNode * node) {
	ASTNode * block = node->data.syncBlock.block;
	double startTime = context->currentTime;
	double endTime = startTime;
	_pushRuntimeScope(context);
	for (ASTList * cell = block->data.block.statements; cell != NULL; cell = cell->next) {
		context->currentTime = startTime;
		_executeNode(context, cell->node);
		if (context->currentTime > endTime) {
			endTime = context->currentTime;
		}
		if (context->errorCount != 0) {
			break;
		}
	}
	_popRuntimeScope(context);
	context->currentTime = endTime;
}

static void _executeNode(InterpreterContext * context, ASTNode * node) {
	if (node == NULL || context->errorCount != 0) {
		return;
	}
	switch (node->nodeType) {
		case AST_PROGRAM:
			_pushRuntimeScope(context);
			_executeList(context, node->data.program.globalDecls);
			_executeNode(context, node->data.program.mainFunc);
			_popRuntimeScope(context);
			break;
		case AST_INCLUDE:
			break;
		case AST_MAIN_FUNC:
			_executeNode(context, node->data.mainFunc.body);
			break;
		case AST_BLOCK:
			_executeBlock(context, node);
			break;
		case AST_DECLARATION:
			_executeDeclaration(context, node);
			break;
		case AST_TRACK_INIT:
			_executeTrackInit(context, node);
			break;
		case AST_ASSIGNMENT:
			_assignRuntimeSymbol(context, node->data.assignment.name, _evaluateExpression(context, node->data.assignment.value));
			break;
		case AST_PLAY:
			_executePlay(context, node);
			break;
		case AST_REST:
			_executeRest(context, node);
			break;
		case AST_SYNC_BLOCK:
			_executeSync(context, node);
			break;
		case AST_CC_STMT:
			_executeCC(context, node);
			break;
		case AST_IF:
			if (_conditionIsTrue(context, node->data.ifStmt.condition, "if condition")) {
				_executeNode(context, node->data.ifStmt.thenBlock);
			} else {
				_executeNode(context, node->data.ifStmt.elseBlock);
			}
			break;
		case AST_FOR:
			_executeFor(context, node);
			break;
		case AST_WHILE:
			_executeWhile(context, node);
			break;
		case AST_BINARY_OP:
		case AST_UNARY_OP:
		case AST_INT_LIT:
		case AST_FLOAT_LIT:
		case AST_IDENTIFIER:
		case AST_PAREN_EXPR:
			_evaluateExpression(context, node);
			break;
	}
}

static void _executeList(InterpreterContext * context, ASTList * list) {
	for (ASTList * cell = list; cell != NULL && context->errorCount == 0; cell = cell->next) {
		_executeNode(context, cell->node);
	}
}

static CompilationStatus _interpretProgram(ASTNode * tree) {
	InterpreterContext context = {0};
	if (tree == NULL) {
		_reportGenerationError(&context, "Cannot generate events from an empty AST.");
	} else {
		_executeNode(&context, tree);
	}
	if (context.errorCount == 0) {
		_printEvents(&context);
	}
	while (context.scope != NULL) {
		_popRuntimeScope(&context);
	}
	_destroyEvents(context.eventsHead);
	_destroyTracks(context.tracks);
	return context.errorCount == 0 ? SUCCEEDED : FAILED;
}

/**
 * Generates the output for one AST node and its children.
 */
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

CompilationStatus executeGenerator(CompilerState * compilerState) {
	logDebugging(_logger, "Generating AST dump and events...");
	ASTNode * tree = (ASTNode *) compilerState->abstractSyntaxtTree;
	if (tree == NULL) {
		printf("(empty AST)\n");
	} else {
		_printNode(tree, 0);
	}
	CompilationStatus status = _interpretProgram(tree);
	fflush(stdout);
	if (status == SUCCEEDED) {
		logDebugging(_logger, "Generation is done.");
	} else {
		logError(_logger, "Generation failed.");
	}
	return status;
}
