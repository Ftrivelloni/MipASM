#ifndef SEMANTIC_ANALYZER_HEADER
#define SEMANTIC_ANALYZER_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdbool.h>
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeSemanticAnalyzerModule();

/**
 * Validates the AST stored in the compiler state before code generation.
 */
CompilationStatus executeSemanticAnalysis(CompilerState * compilerState);

#endif
