# Backend Design Notes

## Current Project Shape

- The compiler reads source from stdin, runs lexical + syntactic analysis, stores the AST root in `CompilerState.abstractSyntaxtTree`, then calls `executeGenerator`.
- The frontend is already domain-shaped, not calculator-shaped. The AST supports:
  - includes
  - global declarations
  - `void main()`
  - blocks and statements
  - declarations, assignments, track initialization
  - `play`, `rest`, `sync`, `set_volume`, `set_pan`, `set_attack`
  - `if`, `for`, `while`
  - arithmetic, relational, logical, identifier, int, and float expressions
- `src/main/c/backend/code-generation/Generator.c` is the only active backend module in `CMakeLists.txt`.
- `Generator.c` currently pretty-prints the AST. It does not emit MipASM/MIDI/assembly yet.
- `src/main/c/backend/domain-specific/Calculator.*` is stale calculator code. It references old AST types and is not compiled.
- `CompilerState` still contains calculator-era fields and TODOs. It should be redesigned once backend data structures are known.

## Editing Constraint

- Do not delete already existing comments while implementing backend changes.

## Program Flow

Frontend:
- `src/main/c/EntryPoint.c` starts the compiler and calls the frontend.
- `src/main/c/frontend/Frontend.c` drives lexical and syntactic analysis.
- `src/main/c/frontend/lexical-analysis/FlexPatterns.l` defines token patterns.
- `src/main/c/frontend/lexical-analysis/FlexActions.c` fills token values.
- `src/main/c/frontend/syntactic-analysis/BisonGrammar.y` validates syntax.
- `src/main/c/frontend/syntactic-analysis/BisonActions.c` builds AST nodes.
- `src/main/c/frontend/syntactic-analysis/AbstractSyntaxTree.*` defines the AST.

Backend:
- `src/main/c/backend/code-generation/Generator.c` currently prints the AST for debugging.
- `src/main/c/backend/code-generation/Generator.h` exposes the generator module API.
- `src/main/c/backend/domain-specific/Calculator.*` exists but is stale and not compiled.

Next Backend Step:
- Add semantic analysis after parsing and before generation.
- Semantic analysis should validate names, types, const assignments, tracks, and domain calls.
- After semantic analysis, code generation can turn the validated AST into final output.
