# MipASM Language Features

MipASM is a C-style language that compiles to a Standard MIDI File (SMF format 1).
This document describes every music statement: how to use it, what it produces in
the MIDI file, and where it is implemented in the compiler.

Compile a program with:

```bash
mipasm examples/ode-to-joy.mip   # writes examples/ode-to-joy.mid
```

## Program structure

```c
#include "<stdlib/audio>"        // directives first

track lead = init_track(0);      // then global declarations
int velocity = 96;

void main() {                    // then exactly one main()
    set_tempo(120);
    play(lead, 60, 1.0);
}
```

Time is measured in **beats** (1 beat = one quarter note = 480 MIDI ticks).
A single time cursor advances as you `play` or `rest` — consecutive statements
happen one after another, even across different tracks. Statements inside a
`sync { ... }` block start at the same time instead.

---

## Tracks and channels

```c
track lead = init_track(0);      // global or inside main(); channel 0..15
```

Each `init_track` creates one MIDI track (an MTrk chunk) bound to a channel.
The track's variable name is written into the file as a Track Name meta event
(`FF 03`), so DAWs show `lead`, `bass`, etc. instead of "Track 1".

> Channel 9 is the General MIDI percussion channel: notes played there are
> drum sounds, not pitches.

## Notes and rests

```c
play(lead, 60, 1.0);   // note 60 (C4), 1 beat long
rest(lead, 0.5);       // advance time half a beat, silently
```

| Argument | Range | Meaning |
|---|---|---|
| note | 0..127 | MIDI note number (60 = middle C; +12 = one octave) |
| duration | > 0 | length in beats (floats allowed: `0.25`, `1.5`) |

`play` emits a Note On (`0x9n`) / Note Off (`0x8n`) pair. The note-on velocity
comes from the global variable `velocity` (default 100), which you can reassign
mid-piece.

## Parallel music: `sync`

```c
sync {                       // every statement starts at the same beat
    play(lead, 76, 4.0);     // a C-major chord across two tracks
    play(bass, 48, 4.0);
}
```

After the block, time continues from the longest statement inside it.

## Tempo: `set_tempo(bpm)`

```c
void main() {
    set_tempo(120);          // initial tempo
    // ... first section ...
    set_tempo(90);           // mid-song change (e.g. a ritardando)
}
```

- `bpm` must be a positive number; `set_tempo(0)` is a compile-time error.
- Allowed anywhere inside `main()` (including loops and `if` blocks), any
  number of times. Each call writes a Set Tempo meta event (`FF 51 03`, value =
  60,000,000 / bpm microseconds per quarter note) at the current beat in the
  conductor track.
- If a program never calls `set_tempo` at beat 0, the compiler writes a default
  **120 BPM** event so playback timing is always explicit.

> The old convention of declaring `const int tempo = 120;` is no longer special:
> it just declares an ordinary variable and does not affect playback.

## Instruments: `set_instrument(track, program)`

```c
set_instrument(lead, 40);    // General MIDI program 40 = violin
set_instrument(bass, 32);    // 32 = acoustic bass
```

Emits a Program Change message (`0xCn`) on the track's channel at the current
beat. `program` is a General MIDI program number, 0..127 (out-of-range values
are clamped with a warning). Without it, every track plays as program 0
(Acoustic Grand Piano). Common programs:

| Program | Instrument | Program | Instrument |
|---|---|---|---|
| 0 | Acoustic Grand Piano | 40 | Violin |
| 24 | Nylon Guitar | 56 | Trumpet |
| 32 | Acoustic Bass | 65 | Alto Sax |
| 33 | Electric Bass | 73 | Flute |

You can call it again mid-song to switch a track's instrument.

## Time signature: `set_time_signature(numerator, denominator)`

```c
set_time_signature(3, 4);    // a waltz
```

- Both arguments must be integers; the denominator must be a power of two in
  1..32 (`1, 2, 4, 8, 16, 32`) — anything else is a compile-time error.
- Emits a Time Signature meta event (`FF 58 04 nn dd 18 08`, where `dd` is
  log2 of the denominator) in the conductor track at the current beat.
  Multiple calls create meter changes.
- This affects how editors display bars and metronomes; it does not change
  how long your beats last (that is `set_tempo`).

## Mixing and expression controllers

All controller statements share the same shape — `set_x(track, value)` with
`value` in 0..127 (clamped with a warning) — and emit a Control Change message
(`0xBn controller value`) at the current beat:

| Statement | MIDI controller | Typical use |
|---|---|---|
| `set_volume(t, v)` | 7 (Channel Volume) | overall track loudness |
| `set_pan(t, v)` | 10 (Pan) | stereo position: 0 left, 64 centre, 127 right |
| `set_attack(t, v)` | 73 (Attack Time) | softer/harder note onset |
| `set_sustain(t, v)` | 64 (Sustain Pedal) | ≥ 64 = pedal down, < 64 = pedal up |
| `set_modulation(t, v)` | 1 (Modulation Wheel) | vibrato depth |
| `set_reverb(t, v)` | 91 (Reverb Send) | room ambience |

```c
set_volume(lead, 110);
set_pan(lead, 84);           // slightly right
set_sustain(lead, 127);      // pedal down
play(lead, 60, 2.0);
set_sustain(lead, 0);        // pedal up
```

## Variables and control flow

The general-purpose layer is deliberately C-like:

```c
const int tempo_intro = 132;             // int, float, boolean, duration
duration eighth = 0.5;

for (int i = 0; i < 8; i = i + 1) {      // for / while / if-else
    play(lead, 60 + i, eighth);
}
```

Everything is evaluated at compile time: loops are unrolled, expressions are
folded, and only the resulting timed events reach the MIDI file.

---

## How it is implemented

Every music statement flows through the same six-stage pipeline:

```
keyword            FlexPatterns.l      "set_tempo" -> SET_TEMPO token
grammar rule       BisonGrammar.y      tempo_stmt: SET_TEMPO ( expression ) ;
AST node           AbstractSyntaxTree.h  AST_TEMPO_STMT { ASTNode * bpm; }
semantic check     SemanticAnalyzer.c  bpm expression must be numeric
execution          Generator.c         _executeTempo(): evaluates bpm at the
                                       current beat, validates, records event
byte emission      MidiEmitter.c       conductor/track chunk encodes the bytes
```

Key files (paths relative to `src/main/c/`):

- `frontend/lexical-analysis/FlexPatterns.l` — keywords.
- `frontend/syntactic-analysis/BisonGrammar.y` — productions; every statement
  starts with a unique keyword, so the grammar stays LALR(1)-conflict-free.
  Tempo/time-signature/instrument statements hang off `statement` only, which
  is what makes them illegal at global scope (a parse error).
- `frontend/syntactic-analysis/AbstractSyntaxTree.h/.c` — node types, the
  `CCKind` enum, constructors/destructors.
- `backend/semantic-analysis/SemanticAnalyzer.c` — declaration/type checks
  (track existence, numeric arguments, int-only time signatures).
- `backend/code-generation/Generator.c` — a compile-time interpreter. It walks
  `main()`, keeps a time cursor (`context->currentTime`), and calls
  a `musicProgramAdd*` builder for every musical statement. Value validation
  that depends on runtime values (positive BPM, power-of-two denominator)
  happens here and fails the compilation via `_reportGenerationError`.
- `backend/code-generation/MidiEmitter.c/.h` — the only module that knows MIDI
  bytes. It holds a flat event list (`MusicEvent`); conductor-level events
  (tempo, time signature) ride the same list with `trackName == NULL`.
  `emitMidiFile()` writes the `MThd` header, then one conductor MTrk (sorted
  tempo/time-signature meta events), then one MTrk per track (track name meta,
  then sorted, delta-encoded channel messages).

### Adding a new `set_*` statement

Follow the six-file pattern (use `set_reverb` as the model for a per-track
controller, or `set_tempo` for a global/conductor statement):

1. **FlexPatterns.l** — add the keyword rule returning a new token.
2. **BisonGrammar.y** — declare the token; add a production (new alternative in
   `cc_stmt` for controllers, or a new rule listed under `statement`).
3. **AbstractSyntaxTree.h/.c** — for controllers just extend `CCKind` (and
   `ccKindName`); otherwise add a node type, union member, destructor case.
4. **SemanticAnalyzer.c** — add a check case (controllers need none).
5. **Generator.c** — map the new `CCKind` in `_executeCC`, or write a new
   `_execute*` + dispatch case.
6. **MidiEmitter.c/.h** — for controllers add a `MidiCCKind` value and its
   controller number in `_controllerNumber` (+ `_ccLabel` for the debug dump);
   otherwise add a `MusicEventKind`, builder, and encoding branch.

Then add an accept test under `test/c/accept/` and a reject test for the new
error paths under `test/c/reject/`.
