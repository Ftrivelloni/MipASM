#include "backend/code-generation/Generator.h"
#include "backend/code-generation/MidiEmitter.h"
#include "backend/semantic-analysis/SemanticAnalyzer.h"
#include "frontend/Frontend.h"
#include "frontend/lexical-analysis/FlexActions.h"
#include "frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "frontend/syntactic-analysis/BisonActions.h"
#include "support/language/String.h"
#include "support/logging/Logger.h"
#include "support/type/CompilationStatus.h"
#include "support/type/CompilerState.h"
#include "support/type/ModuleDestructor.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The real version is injected by CMake from project(VERSION ...). */
#ifndef MIPASM_VERSION
#define MIPASM_VERSION "0.0.0-unknown"
#endif

/**
 * The result of scanning the command line. When "valid" is false an error has
 * already been printed to standard error and the process must exit with
 * FAILED. The help/version flags short-circuit compilation entirely.
 */
typedef struct {
	const char * inputPath;		/* NULL ⇒ read from standard input */
	const char * outputPath;	/* NULL ⇒ derive from the input path */
	bool inputSeen;				/* an input was given, even if it was "-" */
	bool showHelp;
	bool showVersion;
	bool valid;
} CommandLine;

static void _printUsage(FILE * stream) {
	fprintf(stream,
		"Usage: mipasm [options] [file]\n"
		"Compile a MipASM program into a Standard MIDI File.\n"
		"\n"
		"With no <file>, or when <file> is '-', the program is read from standard input.\n"
		"\n"
		"Options:\n"
		"  -o <file>      Write the MIDI output to <file>.\n"
		"                 Default: the input path with its '.mip' suffix replaced by\n"
		"                 '.mid' ('.mid' is appended when there is no '.mip' suffix);\n"
		"                 'a.mid' when reading from standard input.\n"
		"  -h, --help     Print this help and exit.\n"
		"  --version      Print version information and exit.\n"
		"\n"
		"Environment:\n"
		"  LOGGING_LEVEL  Minimum log level: ALL, DEBUGGING, INFORMATION, WARNING,\n"
		"                 ERROR, CRITICAL.\n");
}

static void _printVersion(void) {
	printf("mipasm (MipASM compiler) %s\n", MIPASM_VERSION);
}

/**
 * Scans the command line, C-compiler style: flags may appear anywhere, the
 * single non-flag argument is the input program. Errors (unknown option,
 * missing "-o" filename, multiple inputs) are reported to standard error and
 * flagged through "valid" instead of being silently ignored.
 */
static CommandLine _parseCommandLine(const int length, const char ** arguments) {
	CommandLine commandLine = {
		.inputPath = NULL,
		.outputPath = NULL,
		.inputSeen = false,
		.showHelp = false,
		.showVersion = false,
		.valid = true
	};
	for (int k = 1; k < length; ++k) {
		const char * argument = arguments[k];
		if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
			commandLine.showHelp = true;
		}
		else if (strcmp(argument, "--version") == 0) {
			commandLine.showVersion = true;
		}
		else if (strcmp(argument, "-o") == 0) {
			if (k + 1 >= length) {
				fprintf(stderr, "mipasm: error: missing filename after '-o'\n");
				commandLine.valid = false;
				break;
			}
			commandLine.outputPath = arguments[++k];
		}
		else if (strcmp(argument, "-") == 0 || argument[0] != '-') {
			if (commandLine.inputSeen) {
				fprintf(stderr, "mipasm: error: multiple input files ('%s', '%s')\n",
					commandLine.inputPath == NULL ? "-" : commandLine.inputPath, argument);
				commandLine.valid = false;
				break;
			}
			commandLine.inputSeen = true;
			commandLine.inputPath = strcmp(argument, "-") == 0 ? NULL : argument;
		}
		else {
			fprintf(stderr, "mipasm: error: unrecognized command-line option '%s'\n", argument);
			commandLine.valid = false;
			break;
		}
	}
	if (!commandLine.valid) {
		fprintf(stderr, "Try 'mipasm --help' for more information.\n");
	}
	return commandLine;
}

/**
 * Derives the default MIDI output path from the input path: a trailing ".mip"
 * becomes ".mid" (the suffixes differ only in their final letter, so an input
 * named exactly ".mip" yields ".mid"), any other name gets ".mid" appended,
 * and NULL (standard input) yields "a.mid". The returned string uses
 * heap-memory and must be freed.
 */
static char * _defaultOutputPath(const char * inputPath) {
	if (inputPath == NULL) {
		return concatenate(1, "a.mid");
	}
	const size_t length = strlen(inputPath);
	if (4 <= length && strcmp(inputPath + length - 4, ".mip") == 0) {
		char * outputPath = concatenate(1, inputPath);
		if (outputPath != NULL) {
			outputPath[length - 1] = 'd';
		}
		return outputPath;
	}
	return concatenate(2, inputPath, ".mid");
}

/**
 * The main entry-point of the entire application. If you use "strtok" to
 * parse anything inside this project instead of using Flex and Bison, I will
 * find you, and I will kill you (Bryan Mills; "Taken", 2008).
 *
 * Usage: mipasm [options] [file]
 *   - the input program is read from <file>, or from standard input when no
 *     input path is given (or when <file> is "-");
 *   - the generated Standard MIDI File is written to the "-o" path, defaulting
 *     to the input path with its ".mip" suffix replaced by ".mid" ("a.mid"
 *     when reading from standard input).
 */
const int main(const int length, const char ** arguments) {
	CommandLine commandLine = _parseCommandLine(length, arguments);
	if (!commandLine.valid) {
		return FAILED;
	}
	if (commandLine.showHelp) {
		_printUsage(stdout);
		return SUCCEEDED;
	}
	if (commandLine.showVersion) {
		_printVersion();
		return SUCCEEDED;
	}
	char * derivedOutputPath = NULL;
	if (commandLine.outputPath == NULL) {
		derivedOutputPath = _defaultOutputPath(commandLine.inputPath);
		commandLine.outputPath = derivedOutputPath;
	}
	const char * inputPath = commandLine.inputPath;
	const char * outputPath = commandLine.outputPath;

	LexicalAnalyzer * lexicalAnalyzer = createLexicalAnalyzer();
	Logger * logger = createLogger("EntryPoint");
	for (int k = 0; k < length; ++k) {
		logDebugging(logger, "Argument %d: \"%s\"", k, arguments[k]);
	}
	CompilerState compilerState = {
		.abstractSyntaxtTree = NULL,
		.value = 0,
		.midiOutputPath = outputPath
	};
	ModuleDestructor moduleDestructors[] = {
		initializeAbstractSyntaxTreeModule(),
		initializeFlexActionsModule(lexicalAnalyzer),
		initializeBisonActionsModule(&compilerState),
		initializeFrontendModule(lexicalAnalyzer),
		initializeSemanticAnalyzerModule(),
		initializeGeneratorModule(),
		initializeMidiEmitterModule()
	};

	// Point the scanner at the input file, or fall back to standard input.
	InputBuffer * inputBuffer = NULL;
	CompilationStatus compilationStatus = SUCCEEDED;
	if (inputPath != NULL) {
		inputBuffer = createInputBuffer(lexicalAnalyzer, inputPath);
		if (inputBuffer == NULL) {
			compilationStatus = FAILED;
		}
		else if (inputBuffer->file == NULL) {
			logError(logger, "Cannot open input program '%s'.", inputPath);
			compilationStatus = FAILED;
		}
		else {
			logDebugging(logger, "Compiling '%s' into '%s'...", inputPath, outputPath);
			pushInputBuffer(inputBuffer);
		}
	}
	else {
		logDebugging(logger, "Reading program from standard input; output '%s'.", outputPath);
	}

	if (compilationStatus == SUCCEEDED) {
		compilationStatus = executeSyntacticAnalysis();
		ASTNode * ast = (ASTNode *) compilerState.abstractSyntaxtTree;
		if (compilationStatus != SUCCEEDED) {
			logError(logger, "The syntactic-analysis phase rejects the input program.");
			compilationStatus = FAILED;
		}
		else { // Beginning of the Backend...
			logDebugging(logger, "Frontend complete; analyzing semantics...");
			compilationStatus = executeSemanticAnalysis(&compilerState);

			if (compilationStatus != SUCCEEDED) {
				logError(logger, "The semantic-analysis phase rejects the input program.");
				compilationStatus = FAILED;
			}
			else {
				logDebugging(logger, "Semantic analysis complete; generating output...");
				compilationStatus = executeGenerator(&compilerState);

				if (compilationStatus != SUCCEEDED) {
					logError(logger, "The code-generation phase rejects the input program.");
					// executeGenerator already returns FAILED
				}
			}
			// ...end of the Backend
		}

		logDebugging(logger, "Releasing AST resources...");
		destroyASTNode(ast);
	}

	if (inputBuffer != NULL) {
		destroyInputBuffer(inputBuffer);
	}
	for (int k = (sizeof(moduleDestructors)/sizeof(ModuleDestructor)) - 1; 0 <= k; --k) {
		moduleDestructors[k]();
	}
	logDebugging(logger, "Compilation is done.");
	destroyLogger(logger);
	destroyLexicalAnalyzer(lexicalAnalyzer);
	free(derivedOutputPath);
	return compilationStatus;
}
