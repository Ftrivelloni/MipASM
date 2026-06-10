#include "backend/code-generation/Generator.h"
#include "backend/code-generation/MidiEmitter.h"
#include "backend/semantic-analysis/SemanticAnalyzer.h"
#include "frontend/Frontend.h"
#include "frontend/lexical-analysis/FlexActions.h"
#include "frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "frontend/syntactic-analysis/BisonActions.h"
#include "support/logging/Logger.h"
#include "support/type/CompilationStatus.h"
#include "support/type/CompilerState.h"
#include "support/type/ModuleDestructor.h"
#include <string.h>

/**
 * Scans the command line, C-compiler style. The first non-flag argument is the
 * input program path (NULL ⇒ read from standard input); "-o <path>" sets the
 * MIDI output path. Returns the input path and writes the output path through
 * "outputPath" (left untouched when "-o" is absent, so the caller's default
 * survives).
 */
static const char * _parseArguments(const int length, const char ** arguments, const char ** outputPath) {
	const char * inputPath = NULL;
	for (int k = 1; k < length; ++k) {
		if (strcmp(arguments[k], "-o") == 0) {
			if (k + 1 < length) {
				*outputPath = arguments[++k];
			}
		}
		else if (arguments[k][0] != '-' && inputPath == NULL) {
			inputPath = arguments[k];
		}
	}
	return inputPath;
}

/**
 * The main entry-point of the entire application. If you use "strtok" to
 * parse anything inside this project instead of using Flex and Bison, I will
 * find you, and I will kill you (Bryan Mills; "Taken", 2008).
 *
 * Usage: compiler <input.mip> -o <output.mid>
 *   - the input program is read from <input.mip>, or from standard input when
 *     no input path is given;
 *   - the generated Standard MIDI File is written to <output.mid> (defaults to
 *     "output.mid" when "-o" is omitted).
 */
const int main(const int length, const char ** arguments) {
	LexicalAnalyzer * lexicalAnalyzer = createLexicalAnalyzer();
	Logger * logger = createLogger("EntryPoint");
	for (int k = 0; k < length; ++k) {
		logDebugging(logger, "Argument %d: \"%s\"", k, arguments[k]);
	}
	const char * outputPath = "output.mid";
	const char * inputPath = _parseArguments(length, arguments, &outputPath);
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
	return compilationStatus;
}
