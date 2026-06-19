# MipASM for VS Code

Language support for [MipASM](../../README.md) — a C-style language that
compiles to Standard MIDI Files — for `.mip` source files.

## Features

- **Syntax highlighting** for keywords, types, the domain built-ins
  (`play`, `init_track`, `set_tempo`, …), `#include "<…>"` directives, numbers,
  note names and `stdlib` constants.
- **Snippets**: `main`, `init_track`, `sync`, `play`, `for`, `while`, `if`,
  `set_instrument`, `set_tempo`.
- **Autocompletion** of keywords, built-ins (inserted with argument
  placeholders) and every standard-library constant — dynamics, durations,
  pan, notes (`C0`…`B8`), the 128 General MIDI instruments, the percussion map
  and scale/chord intervals.
- **On-save diagnostics**: the extension runs the `mipasm` compiler, parses its
  `line:column: error:` output and shows red squiggles on the offending line.

## Requirements

A built `mipasm` compiler. The extension finds it via the `mipasm.compilerPath`
setting (default `mipasm`, looked up on `PATH`); when that bare command is not
found it falls back to `.build/mipasm` or `.build-release/mipasm` inside the
workspace folder. Build one from the repository root with `src/main/bash/build.sh`.

## Settings

| Setting | Default | Description |
| --- | --- | --- |
| `mipasm.compilerPath` | `mipasm` | Path to the compiler used for diagnostics. |
| `mipasm.libPath` | `""` | Optional `MIPASM_LIB_PATH` for the standard library. |
| `mipasm.lintOnSave` | `true` | Compile and report diagnostics on save/open. |

## Developing

```bash
npm install
npm run compile      # or: npm run watch
```

Press <kbd>F5</kbd> in VS Code to open an Extension Development Host, then open a
`.mip` file (for example one from the repository's `examples/`). Package a
`.vsix` with `npx @vscode/vsce package`.
