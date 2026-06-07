#include "backend/code-generation/Generator.h"
#include "backend/semantic-analysis/SemanticAnalyzer.h"
#include "frontend/Frontend.h"
#include "frontend/lexical-analysis/FlexActions.h"
#include "frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "frontend/syntactic-analysis/BisonActions.h"
#include "support/logging/Logger.h"
#include "support/type/CompilationStatus.h"
#include "support/type/CompilerState.h"
#include "support/type/ModuleDestructor.h"

/**
 * The main entry-point of the entire application. If you use "strtok" to
 * parse anything inside this project instead of using Flex and Bison, I will
 * find you, and I will kill you (Bryan Mills; "Taken", 2008).
 */
const int main(const int length, const char ** arguments) {
	LexicalAnalyzer * lexicalAnalyzer = createLexicalAnalyzer();
	Logger * logger = createLogger("EntryPoint");
	for (int k = 0; k < length; ++k) {
		logDebugging(logger, "Argument %d: \"%s\"", k, arguments[k]);
	}
	CompilerState compilerState = {
		.abstractSyntaxtTree = NULL,
		.value = 0
	};
	ModuleDestructor moduleDestructors[] = {
		initializeAbstractSyntaxTreeModule(),
		initializeFlexActionsModule(lexicalAnalyzer),
		initializeBisonActionsModule(&compilerState),
		initializeFrontendModule(lexicalAnalyzer),
		initializeSemanticAnalyzerModule(),
		initializeGeneratorModule()
	};
	
	CompilationStatus compilationStatus = executeSyntacticAnalysis();
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
	for (int k = (sizeof(moduleDestructors)/sizeof(ModuleDestructor)) - 1; 0 <= k; --k) {
		moduleDestructors[k]();
	}
	logDebugging(logger, "Compilation is done.");
	destroyLogger(logger);
	destroyLexicalAnalyzer(lexicalAnalyzer);
	return compilationStatus;
}
