#include "BisonActions.h"

/* MODULE INTERNAL STATE */

static CompilerState * _compilerState = NULL;
static Logger * _logger = NULL;

/**
 * The source span of the rule currently being reduced, kept up to date by
 * recordParseLocation() so every newly built node can be stamped with it.
 */
static YYLTYPE _currentLocation = {0};

void recordParseLocation(const YYLTYPE * location) {
	_currentLocation = *location;
}

/**
 * Allocates an AST node and stamps it with the location of the rule being
 * reduced. Every constructor in this file builds its node through here, so all
 * nodes carry a source span for diagnostics.
 */
static ASTNode * _locatedNode(NodeType type) {
	ASTNode * node = newASTNode(type);
	if (node != NULL) {
		node->location.firstLine = _currentLocation.first_line;
		node->location.firstColumn = _currentLocation.first_column;
		node->location.lastLine = _currentLocation.last_line;
		node->location.lastColumn = _currentLocation.last_column;
	}
	return node;
}

/** Shutdown module's internal state. */
void _shutdownBisonActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: BisonActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_compilerState = NULL;
}

ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState) {
	_compilerState = compilerState;
	_logger = createLogger("BisonActions");
	return _shutdownBisonActionsModule;
}

/**
 * Logs a syntactic-analyzer action in DEBUGGING level.
 */
static void _log(const char * functionName) {
	logDebugging(_logger, "%s", functionName);
}

/* TOP-LEVEL */

ASTNode * ProgramSemanticAction(ASTList * includes, ASTList * globalDecls, ASTNode * mainFunc) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_PROGRAM);
	node->data.program.includes = includes;
	node->data.program.globalDecls = globalDecls;
	node->data.program.mainFunc = mainFunc;
	root = node;
	if (_compilerState != NULL) {
		_compilerState->abstractSyntaxtTree = node;
	}
	return node;
}

ASTNode * IncludeSemanticAction(char * path) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_INCLUDE);
	node->data.include.path = path;
	return node;
}

ASTNode * MainFuncSemanticAction(ASTNode * body) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_MAIN_FUNC);
	node->data.mainFunc.body = body;
	return node;
}

ASTNode * BlockSemanticAction(ASTList * statements) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_BLOCK);
	node->data.block.statements = statements;
	return node;
}

/* LIST UTILITY */

ASTList * PrependASTListSemanticAction(ASTNode * head, ASTList * tail) {
	ASTList * cell = (ASTList *) calloc(1, sizeof(ASTList));
	cell->node = head;
	cell->next = tail;
	return cell;
}

/* DECLARATIONS */

ASTNode * DeclarationSemanticAction(int isConst, TypeKind typeKind, char * name, ASTNode * initializer) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_DECLARATION);
	node->data.declaration.isConst = isConst != 0;
	node->data.declaration.typeKind = typeKind;
	node->data.declaration.name = name;
	node->data.declaration.initializer = initializer;
	return node;
}

ASTNode * TrackInitSemanticAction(char * name, ASTNode * channel) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_TRACK_INIT);
	node->data.trackInit.name = name;
	node->data.trackInit.channel = channel;
	return node;
}

/* STATEMENTS */

ASTNode * AssignmentSemanticAction(char * name, ASTNode * value) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_ASSIGNMENT);
	node->data.assignment.name = name;
	node->data.assignment.value = value;
	return node;
}

ASTNode * PlaySemanticAction(char * trackName, ASTNode * note, ASTNode * duration) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_PLAY);
	node->data.play.trackName = trackName;
	node->data.play.note = note;
	node->data.play.duration = duration;
	return node;
}

ASTNode * RestSemanticAction(char * trackName, ASTNode * duration) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_REST);
	node->data.rest.trackName = trackName;
	node->data.rest.duration = duration;
	return node;
}

ASTNode * SyncBlockSemanticAction(ASTNode * block) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_SYNC_BLOCK);
	node->data.syncBlock.block = block;
	return node;
}

ASTNode * CCStmtSemanticAction(CCKind kind, char * trackName, ASTNode * value) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_CC_STMT);
	node->data.ccStmt.kind = kind;
	node->data.ccStmt.trackName = trackName;
	node->data.ccStmt.value = value;
	return node;
}

ASTNode * TempoStmtSemanticAction(ASTNode * bpm) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_TEMPO_STMT);
	node->data.tempoStmt.bpm = bpm;
	return node;
}

ASTNode * TimeSignatureStmtSemanticAction(ASTNode * numerator, ASTNode * denominator) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_TIME_SIGNATURE_STMT);
	node->data.timeSignatureStmt.numerator = numerator;
	node->data.timeSignatureStmt.denominator = denominator;
	return node;
}

ASTNode * InstrumentStmtSemanticAction(char * trackName, ASTNode * program) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_INSTRUMENT_STMT);
	node->data.instrumentStmt.trackName = trackName;
	node->data.instrumentStmt.program = program;
	return node;
}

/* CONTROL FLOW */

ASTNode * IfSemanticAction(ASTNode * condition, ASTNode * thenBlock, ASTNode * elseBlock) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_IF);
	node->data.ifStmt.condition = condition;
	node->data.ifStmt.thenBlock = thenBlock;
	node->data.ifStmt.elseBlock = elseBlock;
	return node;
}

ASTNode * ForSemanticAction(int hasType, TypeKind typeKind, char * initName, ASTNode * initValue,
                            ASTNode * condition, ASTNode * step, ASTNode * body) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_FOR);
	node->data.forStmt.hasType = hasType != 0;
	node->data.forStmt.initTypeKind = typeKind;
	node->data.forStmt.initName = initName;
	node->data.forStmt.initValue = initValue;
	node->data.forStmt.condition = condition;
	node->data.forStmt.step = step;
	node->data.forStmt.body = body;
	return node;
}

ASTNode * WhileSemanticAction(ASTNode * condition, ASTNode * body) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_WHILE);
	node->data.whileStmt.condition = condition;
	node->data.whileStmt.body = body;
	return node;
}

/* EXPRESSIONS */

ASTNode * BinaryOpSemanticAction(OperatorKind op, ASTNode * left, ASTNode * right) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_BINARY_OP);
	node->data.binary.op = op;
	node->data.binary.left = left;
	node->data.binary.right = right;
	return node;
}

ASTNode * UnaryOpSemanticAction(OperatorKind op, ASTNode * operand) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_UNARY_OP);
	node->data.unary.op = op;
	node->data.unary.operand = operand;
	return node;
}

ASTNode * ParenSemanticAction(ASTNode * inner) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_PAREN_EXPR);
	node->data.paren.inner = inner;
	return node;
}

ASTNode * IdentifierSemanticAction(char * name) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_IDENTIFIER);
	node->data.identifier.name = name;
	return node;
}

ASTNode * IntLitSemanticAction(int value) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_INT_LIT);
	node->data.intLit.value = value;
	return node;
}

ASTNode * FloatLitSemanticAction(float value) {
	_log(__FUNCTION__);
	ASTNode * node = _locatedNode(AST_FLOAT_LIT);
	node->data.floatLit.value = value;
	return node;
}
