# MidiEmitter — how it works and how to wire it in

This document explains the new `MidiEmitter` module (`MidiEmitter.h` / `MidiEmitter.c`) and the exact
edits needed to connect it to the rest of the compiler. The module is **self-contained and already
implemented**; the integration steps in Part B are the remaining work.

The emitter has been verified standalone: it produces a byte-correct Standard MIDI File (format 1) and
runs clean under AddressSanitizer. See the worked example at the end.

---

## Part A — How MidiEmitter works

### Where it sits

```
AST ──(SemanticAnalyzer)──► validated AST ──(Generator: compile-time interpreter)──► MusicProgram ──(MidiEmitter)──► .mid file
```

The generator already walks the validated AST keeping a runtime symbol table, evaluates expressions and
control flow, and produces a **timed event stream**. Today it stores that stream as text. The emitter
introduces a *structured* event model (which it owns) and the generator fills it via a builder API.

### The event model (`MidiEmitter.h`)

- `MusicProgram` — the whole piece: a linked list of `MusicEvent`, a declaration-ordered list of
  `MidiTrackInfo`, a `trackCount`, and `tempoBPM` (default 120).
- `MusicEvent` — one timed event (`MUSIC_EVENT_TRACK_INIT | NOTE | REST | CC`) carrying an absolute
  `beat` time, `channel`, borrowed `trackName`, and the per-kind fields (note/velocity/duration, or
  CC kind/value).
- Times are in **beats** (1 beat = 1 quarter note). The emitter converts to MIDI ticks.
- `trackName` pointers are **borrowed** — the emitter never frees them. They point at AST strings, which
  outlive emission (the AST is destroyed in `main` *after* `executeGenerator` returns).

### The builder API (called by the generator)

```c
MusicProgram * createMusicProgram(void);                          // tempoBPM = 120
void musicProgramSetTempo(MusicProgram *, int bpm);
void musicProgramAddTrack(MusicProgram *, name, channel);
void musicProgramAddNote (MusicProgram *, beat, channel, track, note, velocity, durationBeats);
void musicProgramAddRest (MusicProgram *, beat, track, durationBeats);
void musicProgramAddCC   (MusicProgram *, beat, channel, track, MidiCCKind, value, wasFloat, floatValue);
void destroyMusicProgram(MusicProgram *);
void printMusicProgram(const MusicProgram *);                     // reproduces the old `Events:` dump
CompilationStatus emitMidiFile(const MusicProgram *, outputPath);
```

### What `emitMidiFile` produces — Standard MIDI File, format 1

- **Header chunk `MThd`**: length 6, format **1**, ntracks = `1 + trackCount`, division = **480 PPQ**.
- **Conductor track** (first `MTrk`): one tempo meta event `FF 51 03 <µs/qn>` where
  `µs/qn = 60000000 / tempoBPM`, then End-of-Track. This is the idiomatic home for tempo.
- **One `MTrk` per declared `track`** (in declaration order). For that track's events:
  - a `play` becomes a **note-on** (`0x9n note vel`) at its tick and a **note-off** (`0x8n note 0`) at
    `tick + duration`. (Splitting on/off in the emitter keeps all tick math in one place.)
  - a CC becomes `0xBn controller value` — `set_volume`→CC **7**, `set_pan`→CC **10**,
    `set_attack`→CC **73**.
  - a `rest` produces **no** bytes (it only advances time, which the generator already reflected in the
    absolute `beat` of later events).
- **Ordering**: per-track events are sorted by `(tick, order, seq)` where note-offs/CCs (`order 0`) sort
  before note-ons (`order 1`) at the same tick — so a note that ends exactly when an identical note
  starts is not clipped — and `seq` (insertion index) makes the sort stable.
- **Delta-times**: after sorting, absolute ticks become delta-times encoded as MIDI **variable-length
  quantities** (7 bits/byte, high bit = "more bytes follow").
- **Endianness**: every multi-byte header/length field is **big-endian**.
- **Safety**: notes clamp to 0–127, velocity to 1–127, CC values to 0–127, channel masked to 0–15, each
  with a `logWarning` when an adjustment happens. Zero/negative-length notes get a minimum 1-tick length.

### Musical parameters the language doesn't spell out

Per the chosen design, these are supplied without grammar changes:

- **Tempo** — a reserved global variable `tempo` (BPM). The generator passes it via
  `musicProgramSetTempo`. Absent ⇒ 120 BPM. One tempo for the whole file (no mid-piece changes yet).
- **Velocity** — a reserved global variable `velocity` (0–127), read at each `play` so it can change
  over the piece. Absent ⇒ 100.
- **`set_attack`** maps to CC 73 (attack control); its value is an integer 0–127 (the level at which the
  sound starts ramping). Float arguments (e.g. the `0.02` in the tests) are rounded to an int for MIDI;
  the original float is kept only so the textual debug dump stays identical.
- **Instrument** — none emitted (channels default to program 0 / piano). A future `set_instrument` would
  emit a Program Change `0xCn`.

---

## Part B — Integration steps (the remaining edits)

These touch four existing files plus a docs note. None of this is done yet.

### 1. `Generator.c` — fill a `MusicProgram` and emit it

Add the include near the top:
```c
#include "MidiEmitter.h"
```

Give the interpreter a program to fill. In `InterpreterContext` (the struct around line 88) add:
```c
MusicProgram * program;   /* structured events for the MIDI emitter */
```
You can keep the existing text `EventLine` machinery or drop it — the cleanest path is to **replace**
the text events with the structured program and use `printMusicProgram` for the debug dump.

Add a velocity helper (reads the reserved global, falls back to 100):
```c
static int _currentVelocity(InterpreterContext * context) {
    RuntimeSymbol * symbol = _findRuntimeSymbol(context, "velocity");
    if (symbol != NULL && symbol->value.initialized && _isNumericValue(symbol->value)) {
        return (int) _asNumber(symbol->value);
    }
    return 100;
}
```

Capture tempo in `_executeDeclaration` (right after the symbol is declared):
```c
if (strcmp(node->data.declaration.name, "tempo") == 0 && value.initialized && _isNumericValue(value)) {
    musicProgramSetTempo(context->program, (int) _asNumber(value));
}
```

Replace the four `_appendEventLine(...)` calls in the domain executors with builder calls:

- `_executeTrackInit` (after computing `track->channel`):
  ```c
  musicProgramAddTrack(context->program, track->name, track->channel);
  ```
- `_executePlay` (before `context->currentTime += durationNumber;`):
  ```c
  musicProgramAddNote(context->program, context->currentTime, track->channel,
                      track->name, (int) _asNumber(note), _currentVelocity(context), durationNumber);
  ```
- `_executeRest` (before the time advance):
  ```c
  musicProgramAddRest(context->program, context->currentTime, track->name, durationNumber);
  ```
- `_executeCC` (replace the int-vs-float `_appendEventLine` branch):
  ```c
  double ccFloat = _asNumber(value);
  bool wasFloat = (value.type != TYPE_INT);
  int ccInt = (int) (ccFloat >= 0 ? ccFloat + 0.5 : ccFloat - 0.5);
  musicProgramAddCC(context->program, context->currentTime, track->channel, track->name,
                    (MidiCCKind) node->data.ccStmt.kind, ccInt, wasFloat, ccFloat);
  ```
  (`AST CCKind` is `{CC_VOLUME, CC_PAN, CC_ATTACK}`, same order as `MidiCCKind`, so the cast is safe; use
  an explicit `switch` if you prefer not to rely on the ordering.)

Make the program survive the interpreter. Change `_interpretProgram` to hand it back:
```c
static CompilationStatus _interpretProgram(ASTNode * tree, MusicProgram ** outProgram) {
    InterpreterContext context = {0};
    context.program = createMusicProgram();
    if (tree == NULL) {
        _reportGenerationError(&context, "Cannot generate events from an empty AST.");
    } else {
        _executeNode(&context, tree);
    }
    while (context.scope != NULL) { _popRuntimeScope(&context); }
    _destroyTracks(context.tracks);
    if (context.errorCount == 0) {
        printMusicProgram(context.program);   // same `Events:` debug output as before
        *outProgram = context.program;        // caller now owns it
        return SUCCEEDED;
    }
    destroyMusicProgram(context.program);
    *outProgram = NULL;
    return FAILED;
}
```

Call the emitter from `executeGenerator` (after the AST dump):
```c
MusicProgram * program = NULL;
CompilationStatus status = _interpretProgram(tree, &program);
fflush(stdout);
if (status == SUCCEEDED) {
    status = emitMidiFile(program, compilerState->midiOutputPath);
}
if (program != NULL) {
    destroyMusicProgram(program);
}
return status;
```

### 2. `support/type/CompilerState.h` — add the output path

Add one field (leave the `value` field and its joke comment untouched):
```c
const char * midiOutputPath;   /* where to write the .mid; defaults to "output.mid" */
```

### 3. `EntryPoint.c` — read the path from argv and register the module

Include the header:
```c
#include "backend/code-generation/MidiEmitter.h"
```
Initialize the path (`run.sh` already forwards CLI args after stdin):
```c
CompilerState compilerState = {
    .abstractSyntaxtTree = NULL,
    .value = 0,
    .midiOutputPath = (length > 1 && arguments[1] != NULL) ? arguments[1] : "output.mid"
};
```
Register the module in the destructor array (add as the last entry):
```c
initializeGeneratorModule(),
initializeMidiEmitterModule()
```

### 4. `CMakeLists.txt` — compile the new file

Add to the `add_executable(Flex-Bison-Compiler ...)` list:
```cmake
		src/main/c/backend/code-generation/MidiEmitter.c
```

### 5. `IDEA/designe-notes-lolo.md` — document the reserved globals

Note that `tempo` (int BPM) and `velocity` (int 0–127) are recognized global variable names, declared
with the existing grammar, e.g.:
```c
const int tempo = 140;
int velocity = 100;
```

---

## Verify after integrating

```sh
./src/main/bash/build.sh
ASAN_OPTIONS=detect_leaks=0 ./src/main/bash/run.sh test/c/accept/15-full-program out.mid
od -A d -t x1 out.mid | head            # first bytes are "MThd"; format field 00 01
od -A d -t x1 out.mid | grep -o 'MTrk' | wc -l   # == 1 + number of init_track in the source
./src/main/bash/test.sh                 # existing accept/reject exit-code tests still pass
```

Good fixtures: `23-generation-sync` (two simultaneous note-ons → polyphony across track chunks) and a CC
test. If a player is installed, `timidity out.mid` for an audible check.

---

## Worked example (real output of the implemented emitter)

A program with `tempo = 140`, tracks `lead` (ch 0) and `bass` (ch 1), a volume CC, a few notes
including an overlapping `lead`/`bass` chord at beat 2, a rest, a `set_attack`, and an out-of-range note
(200) produced this exact file:

```
MThd: 4d 54 68 64 00 00 00 06 00 01 00 03 01 e0
  └ format=1, ntracks=3, division=480 (0x01e0)
MTrk(conductor): ... 00 ff 51 03 06 8a 1b 00 ff 2f 00
  └ tempo 0x068a1b = 428571 µs/qn = 140 BPM, then End-of-Track
MTrk(lead, ch0):
  00 b0 07 64           delta 0, CC volume=100
  00 90 3c 64           delta 0, note-on  60 vel 100
  83 60 80 3c 00        delta 480 (=1 beat), note-off 60
  00 90 40 64  83 60 80 40 00     note 64 for one beat
  00 90 3c 64  83 60 80 3c 00     note 60 at beat 2
  83 60 90 7f 64  83 60 80 7f 00  note 200 -> clamped to 0x7f (127), with a warning
  00 ff 2f 00          End-of-Track
MTrk(bass, ch1):
  87 40 91 24 64        delta 960 (=beat 2), note-on 36 (chord with lead)
  83 60 81 24 00        delta 480, note-off 36
  00 b1 49 02           CC 73 (attack) = 2  (the 0.02 float, rounded)
  00 ff 2f 00          End-of-Track
```

Note how the beat-2 `lead` and `bass` notes land in **separate** track chunks (true polyphony), the VLQ
`83 60` decodes to 480 ticks, and the rest contributes no bytes.
