#include "FlexActions.h"
#include "../../support/configuration/LibraryLocator.h"
#include <stdio.h>

/* MODULE INTERNAL STATE */

/** A standard library loaded through an #include directive. The canonical
 * path is the include-once deduplication key; the input buffer is retained
 * until shutdown because yypop_buffer_state does not close the file. */
typedef struct {
	char * canonicalPath;
	InputBuffer * inputBuffer;
} LoadedLibrary;

static bool _logIgnoredLexemes = true;
static InputBuffer * _inputBuffer = NULL;
static LexicalAnalyzer * _lexicalAnalyzer = NULL;
static Logger * _logger = NULL;
static LoadedLibrary * _loadedLibraries = NULL;
static size_t _loadedLibraryCount = 0;
static size_t _loadedLibraryCapacity = 0;
static char * _libraryRoot = NULL;
static bool _libraryRootResolved = false;

/** Shutdown module's internal state. */
void _shutdownFlexActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: FlexActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	if (_inputBuffer != NULL) {
		destroyInputBuffer(_inputBuffer);
		_inputBuffer = NULL;
	}
	for (size_t k = 0; k < _loadedLibraryCount; ++k) {
		destroyInputBuffer(_loadedLibraries[k].inputBuffer);
		free(_loadedLibraries[k].canonicalPath);
	}
	free(_loadedLibraries);
	_loadedLibraries = NULL;
	_loadedLibraryCount = 0;
	_loadedLibraryCapacity = 0;
	free(_libraryRoot);
	_libraryRoot = NULL;
	_libraryRootResolved = false;
	_lexicalAnalyzer = NULL;
}

ModuleDestructor initializeFlexActionsModule(LexicalAnalyzer * lexicalAnalyzer) {
	_inputBuffer = NULL;
	_lexicalAnalyzer = lexicalAnalyzer;
	_logger = createLogger("FlexActions");
	_logIgnoredLexemes = getBooleanOrDefault("LOG_IGNORED_LEXEMES", _logIgnoredLexemes);
	_loadedLibraries = NULL;
	_loadedLibraryCount = 0;
	_loadedLibraryCapacity = 0;
	_libraryRoot = NULL;
	_libraryRootResolved = false;
	return _shutdownFlexActionsModule;
}

/* PRIVATE HELPERS */

/**
 * Logs a lexical-analyzer action over a token in DEBUGGING level.
 */
static void _logTokenAction(const char * actionName, Token * token) {
	char * _lexeme = escape(token->lexeme);
	logDebugging(_logger, WARNING_COLOR "%s" DEFAULT_COLOR ": Token(context=%d, label=%d, length=%d, lexeme=%s\"%s\"%s, line=%d, semanticValue=%p)",
		actionName,
		token->context,
		token->label,
		token->length,
		INFORMATION_COLOR, _lexeme, DEFAULT_COLOR,
		token->line,
		token->semanticValue);
	free(_lexeme);
	_lexeme = NULL;
}

static CompilationStatus _pushSimpleToken(const char * actionName, TokenLabel label) {
	Token * token = createToken(_lexicalAnalyzer, label);
	_logTokenAction(actionName, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

/* PUBLIC FUNCTIONS */

CompilationStatus KeywordLexemeAction(TokenLabel label) {
	return _pushSimpleToken(__FUNCTION__, label);
}

CompilationStatus PunctuationLexemeAction(TokenLabel label) {
	return _pushSimpleToken(__FUNCTION__, label);
}

CompilationStatus OperatorLexemeAction(TokenLabel label) {
	return _pushSimpleToken(__FUNCTION__, label);
}

CompilationStatus ArithmeticOperatorLexemeAction(TokenLabel label) {
	return _pushSimpleToken(__FUNCTION__, label);
}

CompilationStatus ParenthesisLexemeAction(TokenLabel label) {
	return _pushSimpleToken(__FUNCTION__, label);
}

// This is for braces; yes, it should be similar to parenthesis ^^^^
CompilationStatus BraceLexemeAction(TokenLabel label) {
	return _pushSimpleToken(__FUNCTION__, label);
}

CompilationStatus IdentifierLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, ID);
	token->semanticValue->string = strdup(token->lexeme);
	_logTokenAction(__FUNCTION__, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

CompilationStatus IntegerLiteralLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, INT_LIT);
	token->semanticValue->integer = atoi(token->lexeme);
	_logTokenAction(__FUNCTION__, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

CompilationStatus FloatLiteralLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, FLOAT_LIT);
	token->semanticValue->floatValue = (float) atof(token->lexeme);
	_logTokenAction(__FUNCTION__, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

CompilationStatus StringLiteralLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, STRING_LIT);
	/* The lexeme matches "<...>"; strip the surrounding double-quotes. */
	unsigned int rawLen = token->length;
	if (rawLen >= 2 && token->lexeme[0] == '"' && token->lexeme[rawLen - 1] == '"') {
		unsigned int innerLen = rawLen - 2;
		char * stripped = (char *) calloc(innerLen + 1, sizeof(char));
		memcpy(stripped, token->lexeme + 1, innerLen);
		stripped[innerLen] = '\0';
		token->semanticValue->string = stripped;
	} else {
		token->semanticValue->string = strdup(token->lexeme);
	}
	_logTokenAction(__FUNCTION__, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context) {
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, OPEN_COMMENT);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	enterLexicalAnalyzerContext(_lexicalAnalyzer, context);
	return IN_PROGRESS;
}

CompilationStatus LeaveMultilineCommentLexemeAction() {
	leaveLexicalAnalyzerContext(_lexicalAnalyzer);
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, CLOSE_COMMENT);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	return IN_PROGRESS;
}

CompilationStatus IgnoredLexemeAction() {
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, IGNORED);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	return IN_PROGRESS;
}

CompilationStatus EOFLexemeAction() {
	CompilationStatus status = IN_PROGRESS;
	Token * token = createToken(_lexicalAnalyzer, 0);
	_logTokenAction(__FUNCTION__, token);
	if (!popInputBuffer(_lexicalAnalyzer)) {
		status = pushToken(_lexicalAnalyzer, token);
		FlexContext context = currentLexicalAnalyzerContext(_lexicalAnalyzer);
		if (0 < context) {
			logError(_logger, "The final context is not closed (context=%d).", context);
			status = FAILED;
		}
	}
	destroyToken(token);
	return status;
}

CompilationStatus UnknownLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, UNKNOWN);
	_logTokenAction(__FUNCTION__, token);
	destroyToken(token);
	return FAILED;
}

/** Returns true when the canonical path is already in the loaded set. */
static bool _isLibraryLoaded(const char * canonicalPath) {
	for (size_t k = 0; k < _loadedLibraryCount; ++k) {
		if (strcmp(_loadedLibraries[k].canonicalPath, canonicalPath) == 0) {
			return true;
		}
	}
	return false;
}

/** Adds a library to the loaded set; returns false on allocation failure. */
static bool _registerLoadedLibrary(char * canonicalPath, InputBuffer * inputBuffer) {
	if (_loadedLibraryCount == _loadedLibraryCapacity) {
		size_t newCapacity = _loadedLibraryCapacity == 0 ? 8 : _loadedLibraryCapacity * 2;
		LoadedLibrary * grown = (LoadedLibrary *) realloc(_loadedLibraries, newCapacity * sizeof(LoadedLibrary));
		if (grown == NULL) {
			return false;
		}
		_loadedLibraries = grown;
		_loadedLibraryCapacity = newCapacity;
	}
	_loadedLibraries[_loadedLibraryCount].canonicalPath = canonicalPath;
	_loadedLibraries[_loadedLibraryCount].inputBuffer = inputBuffer;
	++_loadedLibraryCount;
	return true;
}

CompilationStatus IncludeDirectiveLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, INCLUDE);
	_logTokenAction(__FUNCTION__, token);
	CompilationStatus status = IN_PROGRESS;
	/* The lexeme is the whole directive: #include "<name>". */
	const char * open = strchr(token->lexeme, '<');
	const char * close = strrchr(token->lexeme, '>');
	size_t nameLength = (size_t) (close - open) - 1;
	char * name = (char *) calloc(nameLength + 1, sizeof(char));
	if (name == NULL) {
		destroyToken(token);
		return FAILED;
	}
	memcpy(name, open + 1, nameLength);

	if (!_libraryRootResolved) {
		_libraryRoot = resolveLibraryRoot();
		_libraryRootResolved = true;
	}
	if (_libraryRoot == NULL) {
		/* Plain stderr (not the logger) so the failure is visible at any
		   LOGGING_LEVEL, matching the CLI's error style. */
		fprintf(stderr, "mipasm: fatal error: cannot locate the MipASM library directory; set MIPASM_LIB_PATH\n");
		status = FAILED;
	}
	else {
		char * canonicalPath = resolveLibraryFile(_libraryRoot, name);
		if (canonicalPath == NULL) {
			fprintf(stderr, "mipasm: fatal error: cannot open library '<%s>': no such file '%s/%s.mip'\n",
				name, _libraryRoot, name);
			status = FAILED;
		}
		else if (_isLibraryLoaded(canonicalPath)) {
			logDebugging(_logger, "Library '<%s>' is already loaded; skipping.", name);
			free(canonicalPath);
		}
		else {
			InputBuffer * libraryBuffer = createInputBuffer(_lexicalAnalyzer, canonicalPath);
			if (libraryBuffer == NULL || libraryBuffer->file == NULL
				|| !_registerLoadedLibrary(canonicalPath, libraryBuffer)) {
				fprintf(stderr, "mipasm: fatal error: cannot read library file '%s'\n", canonicalPath);
				destroyInputBuffer(libraryBuffer);
				free(canonicalPath);
				status = FAILED;
			}
			else {
				logDebugging(_logger, "Loading library '<%s>' from '%s'.", name, canonicalPath);
				pushInputBuffer(libraryBuffer);
			}
		}
	}
	free(name);
	destroyToken(token);
	return status;
}
