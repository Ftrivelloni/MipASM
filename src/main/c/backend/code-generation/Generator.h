#ifndef GENERATOR_HEADER
#define GENERATOR_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/language/String.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdarg.h>
#include <stdio.h>

/** Initialize module's internal state. */
ModuleDestructor initializeGeneratorModule();

/**
 * Pretty-prints the AST stored in the compiler state to standard output.
 * The frontend produces an AST; this dump is the proof that the tree was
 * built end-to-end.
 */
void executeGenerator(CompilerState * compilerState);

#endif
