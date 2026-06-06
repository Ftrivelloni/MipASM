# Backend Design Notes

## Current Project Shape

- The current `run.sh` script pipes the source file into the compiler's standard input. `EntryPoint.c` only logs command-line arguments; it does not currently open an input path itself.
- The compiler runs lexical + syntactic analysis, stores the AST root in `CompilerState.abstractSyntaxtTree`, runs semantic analysis, then calls `executeGenerator` only if semantics succeed.
- The frontend is already domain-shaped, not calculator-shaped. The AST supports:
  - includes
  - global declarations
  - `void main()`
  - blocks and statements
  - declarations, assignments, track initialization
  - `play`, `rest`, `sync`, `set_volume`, `set_pan`, `set_attack`
  - `if`, `for`, `while`
  - arithmetic, relational, logical, identifier, int, and float expressions
- `src/main/c/backend/semantic-analysis/SemanticAnalyzer.c` and `src/main/c/backend/code-generation/Generator.c` are active backend modules in `CMakeLists.txt`.
- `SemanticAnalyzer.c` validates scoped names, duplicate declarations, type compatibility, const assignments, track usage, and domain call argument types.
- `Generator.c` currently pretty-prints the AST. It does not emit MipASM/MIDI/assembly yet.
- `src/main/c/backend/domain-specific/Calculator.*` is stale calculator code. It references old AST types and is not compiled.
- `CompilerState` still contains calculator-era fields and TODOs. It should be redesigned once backend data structures are known.
- The checked-in `.build/Flex-Bison-Compiler` may be stale. Rebuild before using test results to judge current source behavior.
- AddressSanitizer is enabled in `CMakeLists.txt`; in this execution environment, LeakSanitizer can force nonzero exits even for accepted programs. Use `ASAN_OPTIONS=detect_leaks=0` when separating compiler behavior from sanitizer environment issues.

## Editing Constraint

- Do not delete already existing comments while implementing backend changes.

## Program Flow

Frontend:
- `src/main/c/EntryPoint.c` starts the compiler and calls the frontend.
- `src/main/bash/run.sh` accepts a file path, then pipes that file into `.build/Flex-Bison-Compiler`.
- `src/main/c/frontend/Frontend.c` drives lexical and syntactic analysis.
- `src/main/c/frontend/lexical-analysis/FlexPatterns.l` defines token patterns.
- `src/main/c/frontend/lexical-analysis/FlexActions.c` fills token values.
- `src/main/c/frontend/syntactic-analysis/BisonGrammar.y` validates syntax.
- `src/main/c/frontend/syntactic-analysis/BisonActions.c` builds AST nodes.
- `src/main/c/frontend/syntactic-analysis/AbstractSyntaxTree.*` defines the AST.

Backend:
- `src/main/c/backend/semantic-analysis/SemanticAnalyzer.c` validates the parsed AST before generation.
- `src/main/c/backend/semantic-analysis/SemanticAnalyzer.h` exposes the semantic analyzer module API.
- `src/main/c/backend/code-generation/Generator.c` currently prints the AST for debugging.
- `src/main/c/backend/code-generation/Generator.h` exposes the generator module API.
- `src/main/c/backend/domain-specific/Calculator.*` exists but is stale and not compiled.

Current Backend Baseline:
- Semantic analysis is already added after parsing and before generation.
- The semantic analyzer keeps its symbol table internally for one pass; it does not yet persist backend-ready data in `CompilerState`.
- `Generator.c` still receives the AST directly and emits an AST dump, not final MipASM/MIDI/assembly.

Next Backend Step:
- Rebuild from current source and rerun tests before treating failures as source failures.
- Tighten semantic rules needed by the real backend, such as MIDI channel range, note range, CC value ranges, positive durations, and exact `duration` compatibility rules.
- Decide the first backend representation for validated musical events: tracks, channels, play/rest events, control changes, timing, and `sync` behavior.
- Redesign `CompilerState` once that representation is chosen, replacing calculator-era fields with backend data structures.
- Replace the AST dump generator with a minimal real output target, preferably a textual event/MipASM listing first, then MIDI/assembly once event timing is stable.
