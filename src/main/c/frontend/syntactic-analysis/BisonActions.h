#ifndef BISON_ACTIONS_HEADER
#define BISON_ACTIONS_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonParser.h"
#include <stdbool.h>
#include <stdlib.h>

ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState);

/* Top-level. */
ASTNode * ProgramSemanticAction(ASTList * includes, ASTList * globalDecls, ASTNode * mainFunc);
ASTNode * IncludeSemanticAction(char * path);
ASTNode * MainFuncSemanticAction(ASTNode * body);
ASTNode * BlockSemanticAction(ASTList * statements);

/* List utility. */
ASTList * PrependASTListSemanticAction(ASTNode * head, ASTList * tail);

/* Declarations. */
ASTNode * DeclarationSemanticAction(int isConst, TypeKind typeKind, char * name, ASTNode * initializer);
ASTNode * TrackInitSemanticAction(char * name, ASTNode * channel);

/* Statements. */
ASTNode * AssignmentSemanticAction(char * name, ASTNode * value);
ASTNode * PlaySemanticAction(char * trackName, ASTNode * note, ASTNode * duration);
ASTNode * RestSemanticAction(char * trackName, ASTNode * duration);
ASTNode * SyncBlockSemanticAction(ASTNode * block);
ASTNode * CCStmtSemanticAction(CCKind kind, char * trackName, ASTNode * value);

/* Control flow. */
ASTNode * IfSemanticAction(ASTNode * condition, ASTNode * thenBlock, ASTNode * elseBlock);
ASTNode * ForSemanticAction(int hasType, TypeKind typeKind, char * initName, ASTNode * initValue,
                            ASTNode * condition, ASTNode * step, ASTNode * body);
ASTNode * WhileSemanticAction(ASTNode * condition, ASTNode * body);

/* Expressions. */
ASTNode * BinaryOpSemanticAction(OperatorKind op, ASTNode * left, ASTNode * right);
ASTNode * UnaryOpSemanticAction(OperatorKind op, ASTNode * operand);
ASTNode * ParenSemanticAction(ASTNode * inner);
ASTNode * IdentifierSemanticAction(char * name);
ASTNode * IntLitSemanticAction(int value);
ASTNode * FloatLitSemanticAction(float value);

#endif
