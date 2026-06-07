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
- `Generator.c` currently pretty-prints the AST and then interprets the validated AST at compile time to emit a concrete textual event stream. It does not emit MIDI/assembly yet.
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
- `src/main/c/backend/code-generation/Generator.c` prints the AST for debugging, then executes the program as a compile-time interpreter and emits textual events.
- `src/main/c/backend/code-generation/Generator.h` exposes the generator module API.
- `src/main/c/backend/domain-specific/Calculator.*` exists but is stale and not compiled.

Current Backend Baseline:
- Semantic analysis is already added after parsing and before generation.
- The semantic analyzer keeps its symbol table internally for one pass; it does not yet persist backend-ready data in `CompilerState`.
- `Generator.c` receives the AST directly, keeps its own scoped runtime symbol table, evaluates expressions, executes control flow, and appends an `Events:` section after the AST dump.
- Generator runtime values track both type and initialization state. Declared variables without initializers are uninitialized, and using them during generation rejects the program instead of silently defaulting to `0`.
- The compile-time interpreter supports declarations, assignments, track initialization, `play`, `rest`, CC statements, `if`, `for`, `while`, and `sync`.
- Loop execution is capped at 100000 iterations. Exceeding that limit rejects generation.
- `sync` executes each child statement from the same start time, then advances the current time cursor by the maximum child duration.
- `executeGenerator` now returns `CompilationStatus`; `EntryPoint.c` treats generator failure as a failed compilation.

Next Backend Step:
- Rebuild from current source and rerun tests before treating failures as source failures.
- Tighten semantic rules needed by the real backend, such as MIDI channel range, note range, CC value ranges, positive durations, and exact `duration` compatibility rules.
- Decide whether the textual event stream should remain direct stdout output or become a structured backend representation stored outside the generator before MIDI/assembly emission.
- Redesign `CompilerState` once backend data ownership is chosen, replacing calculator-era fields with event/track generation state if needed.
- Replace or supplement the debug AST dump once the event stream is stable enough to become the main backend output.
