# The MipASM Programming Guide

MipASM is a small, C-style programming language whose programs *are* pieces of
music. Instead of producing an executable, the compiler interprets your program
at compile time and writes a **Standard MIDI File** (`.mid`) that any media
player, DAW, or synthesizer can play.

This guide teaches the language from scratch. For a statement-by-statement
reference (exact MIDI bytes, implementation map, how to extend the compiler),
see [LANGUAGE_FEATURES.md](LANGUAGE_FEATURES.md).

```c
// hello.mip — your first program: one note of C major scale per beat.
track piano = init_track(0);

void main() {
    set_tempo(120);
    play(piano, 60, 1.0);   // C4
    play(piano, 62, 1.0);   // D4
    play(piano, 64, 1.0);   // E4
    play(piano, 65, 1.0);   // F4
    play(piano, 67, 2.0);   // G4, held two beats
}
```

---

## 1. Getting started

Build the compiler once (needs `cmake`, `make`, `gcc`, `flex` and `bison`):

```bash
./src/main/bash/build.sh
```

Compile a program and play the result:

```bash
mipasm hello.mip
# writes hello.mid next to the input; open it in any media player / DAW,
# or: timidity hello.mid
```

(Until you install the compiler, `./src/main/bash/run.sh` runs the development
build with exactly the same arguments.)

- The input is a file argument; `-o <path>` chooses the output. Without `-o`,
  the output is the input path with its `.mip` suffix replaced by `.mid`.
- Without a file argument — or with `-` — the compiler reads from stdin and
  writes `a.mid` by default.
- `mipasm --help` lists every option; `mipasm --version` prints the version.
- The exit status is `0` when compilation succeeds, non-zero otherwise.
- Run the whole test suite with `./src/main/bash/test.sh`.
- Set `LOGGING_LEVEL=DEBUGGING` to print the AST and the generated event list,
  which is the fastest way to see *when* every note happens:

```bash
LOGGING_LEVEL=DEBUGGING mipasm hello.mip
```

To install the `mipasm` command system-wide (a release build, without the
development sanitizers):

```bash
cmake -S . -B .build-release -DMIPASM_SANITIZE=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build .build-release
sudo cmake --install .build-release
```

## 2. Anatomy of a program

A program has three sections, in this order:

```c
#include "<stdlib/audio>"          // 1. directives (zero or more)

const int verse_len = 8;           // 2. global declarations:
track lead = init_track(0);        //    variables and tracks

void main() {                      // 3. exactly one main(), no arguments
    // statements...
}
```

1. **Directives** — each `#include "<name>"` loads the library `name.mip` from
   the compiler's library directory, splicing its declarations into your
   program as if you had written them there. A library is loaded at most once
   (repeated or circular includes are skipped), libraries may include other
   libraries, and including a library that does not exist is a fatal error.
2. **Globals** — variable declarations and `init_track` calls. Globals are
   visible everywhere in `main()`.
3. **`main()`** — the piece itself. All music statements live here (or in
   blocks nested inside it). Declaring tracks inside `main()` is also allowed.

Comments are exactly like C: `// to end of line` and `/* block */`.

> **Mental model:** the compiler *runs* your program top to bottom at compile
> time. Loops are unrolled, variables are evaluated, and every `play`, `rest`,
> `set_*` call is recorded as a timed event. The MIDI file is the trace of that
> run — there is no runtime.

### The standard library

Six libraries of named constants ship with the compiler, so you can write
`set_instrument(lead, VIOLIN)` instead of remembering that violins are
program 40:

| Library | Provides | Sample names |
|---|---|---|
| `<stdlib/audio>` | dynamics (velocities), note lengths (`duration`), pan | `PIANISSIMO`..`FORTISSIMO`, `WHOLE` `HALF` `QUARTER` `EIGHTH` `SIXTEENTH`, `LEFT` `CENTER` `RIGHT` |
| `<stdlib/instruments>` | all 128 General MIDI programs | `ACOUSTIC_GRAND_PIANO`, `VIOLIN`, `TRUMPET`, `FLUTE`, `ALTO_SAX` |
| `<stdlib/drums>` | the GM percussion map (notes for channel 9) | `DRUM_CHANNEL`, `KICK`, `SNARE`, `HIHAT`, `CRASH`, `RIDE` |
| `<stdlib/notes>` | note numbers, octaves 0..8 | `C4` (middle C, 60), `A4` (440 Hz, 69) — sharps use `S`: `CS4` is C#4 |
| `<stdlib/scales>` | scale intervals in semitones | `MINOR_THIRD`, `PERFECT_FIFTH`, `OCTAVE`, `WHOLE_STEP` |
| `<stdlib/chords>` | chord-tone intervals (includes scales) | `DOMINANT_SEVENTH`, `MAJOR_NINTH`, `FLAT_FIFTH` |

```c
#include "<stdlib/instruments>"
#include "<stdlib/notes>"
#include "<stdlib/audio>"

track lead = init_track(0);

void main() {
    set_instrument(lead, FLUTE);
    play(lead, C4, QUARTER);                  // middle C, one beat
    play(lead, C4 + 12, HALF);                // an octave up, two beats
}
```

The libraries are plain MipASM files — open them to see every name. The
compiler looks for them next to its own executable (`lib/` in a development
checkout, `share/mipasm/lib/` when installed); set `MIPASM_LIB_PATH` to use a
library directory somewhere else.

## 3. Values and types

| Type | Example | Holds |
|---|---|---|
| `int` | `int x = 60;` | whole numbers |
| `float` | `float swing = 0.66;` | decimals (literals: `0.5`, `.25`) |
| `duration` | `duration eighth = 0.5;` | note lengths in beats |
| `boolean` | `boolean loud = 1;` | truth values (from ints: 0 = false) |
| `track` | `track lead = init_track(0);` | a MIDI track handle |

- `const` makes a variable immutable: `const int chorus = 4;`. Assigning to a
  `const` is a compile error.
- Variables must be declared before use, and may not be redeclared in the same
  scope (shadowing in an inner block is fine).
- Using an uninitialized variable in a musical statement is an error.

**Implicit conversions** (everything else is rejected):

- `int` → `float`, `int` → `boolean`, `int` or `float` → `duration`.

**There are no boolean literals** — use `0` and `1`, or any comparison:
`boolean repeat = verse < 4;`

## 4. Expressions

Operators, from lowest to highest precedence (same as C):

| Precedence | Operators |
|---|---|
| lowest | `\|\|` |
| | `&&` |
| | `==` `!=` |
| | `<` `<=` `>` `>=` |
| | `+` `-` |
| | `*` `/` |
| highest | `!` (unary), `( ... )` |

- Arithmetic needs numeric operands (`int`, `float`, `duration`). The result is
  `float` if either side is `float`, else `duration` if either side is a
  `duration`, else `int`.
- **Integer division truncates**: `7 / 2` is `3`, but `7 / 2.0` is `3.5`.
- Comparisons yield `boolean`. Tracks can be compared with `==`/`!=` (same
  track or not); `&&`, `\|\|`, `!` need boolean-compatible operands.
- There is no `%`, no `+=`/`++`, and assignment is a statement, not an
  expression — write `i = i + 1;`.

## 5. Control flow

Exactly the C shapes you expect, with mandatory braces:

```c
if (rep == 0) {
    velocity = 96;
} else {
    velocity = 118;
}

for (int i = 0; i < 12; i = i + 1) {     // declares i, or reuse: for (i = 0; ...)
    play(lead, 55 + i, 0.25);
}

while (rep < 2) {
    rep = rep + 1;
}
```

Because the program executes at compile time, a loop that plays 8 notes simply
writes 8 notes into the MIDI file. Runaway loops are caught: any loop that
exceeds **100,000 iterations** aborts compilation with an error.

## 6. The time model

The compiler keeps **one time cursor**, measured in **beats** (quarter notes).

- `play(track, note, duration)` records a note at the cursor, then advances the
  cursor by `duration`.
- `rest(track, duration)` advances the cursor silently.
- Every other statement (assignments, `set_*`, control flow) takes zero time.

Consecutive statements therefore happen one after another — *even on different
tracks*. To make things happen **at the same time**, use `sync`:

```c
sync {                          // each statement starts at the same beat
    play(lead, 64, 2.0);        // a two-beat melody note...
    play(bass, 48, 1.0);        // ...over a one-beat bass note
}
// the cursor now sits after the LONGEST statement in the block (2 beats)
```

A `sync` block can contain any statements, including loops; each top-level
statement in the block is rewound to the block's start beat.

Chords are just a `sync` of `play`s on one track:

```c
sync { play(piano, 60, 2.0); play(piano, 64, 2.0); play(piano, 67, 2.0); }
```

## 7. Tracks, notes, and loudness

### Tracks

```c
track lead = init_track(0);     // channel 0..15
```

A track is one instrument line: it becomes its own track in the MIDI file
(named after the variable, so editors show "lead"), bound to a MIDI channel.
Use one channel per track. **Channel 9 is special**: General MIDI reserves it
for percussion — note numbers there select drums (36 kick, 38 snare, 42 closed
hi-hat) instead of pitches.

### Note numbers

`play` takes a MIDI note number 0..127. Middle C (C4) is **60**; each step is
a semitone, +12 is one octave:

| Note | C | C# | D | D# | E | F | F# | G | G# | A | A# | B |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Octave 4 | 60 | 61 | 62 | 63 | 64 | 65 | 66 | 67 | 68 | 69 | 70 | 71 |

(`A4 = 69` is the 440 Hz tuning reference. For other octaves add/subtract 12.)

Since notes are just ints, melodies can be computed:

```c
for (int i = 0; i < 12; i = i + 1) {
    play(lead, 55 + i, 0.2);    // a rising chromatic run
}
```

### Velocity (how hard notes are struck)

The reserved global `velocity` (1..127, default 100) sets the note-on velocity
of every subsequent `play`. Reassign it for dynamics:

```c
int velocity = 96;      // mezzo-forte
// ...
velocity = 118;         // fortissimo for the repeat
```

## 8. Shaping the sound

These statements all take effect *at the current beat*, so you can automate
them over time. Values are 0..127 unless noted.

### Tempo and meter (global — no track argument)

```c
set_tempo(120);              // beats per minute; must be positive
set_time_signature(3, 4);    // denominator: power of two in 1..32
```

Both may appear any number of times — `set_tempo(90)` halfway through the piece
is a real mid-song tempo change. A program that never sets a tempo plays at
120 BPM.

### Instruments

```c
set_instrument(lead, 40);    // General MIDI program number 0..127
```

Without it, every track is program 0 (Acoustic Grand Piano). A few useful
General MIDI programs: 0 piano, 24 nylon guitar, 32 acoustic bass, 33 electric
bass, 40 violin, 48 string ensemble, 56 trumpet, 65 alto sax, 73 flute. (Search
"General MIDI program numbers" for the full table of 128.)

### Mixing and expression

```c
set_volume(lead, 110);       // track loudness (CC 7)
set_pan(lead, 84);           // stereo: 0 left, 64 centre, 127 right (CC 10)
set_attack(harmony, 20);     // note onset hardness (CC 73)
set_sustain(lead, 127);      // pedal: >= 64 down, < 64 up (CC 64)
set_modulation(lead, 40);    // vibrato depth (CC 1)
set_reverb(harmony, 60);     // room ambience (CC 91)
```

`velocity` vs `set_volume`: velocity is per-note (how hard the key is hit);
volume is the track's fader. Use velocity for accents and phrasing, volume for
the overall mix.

## 9. A complete worked example

```c
// waltz.mip — an 8-bar waltz demonstrating most of the language.
#include "<stdlib/audio>"

int velocity = 100;

track melody = init_track(0);
track accomp = init_track(1);

void main() {
    set_tempo(140);
    set_time_signature(3, 4);          // a waltz: 3 beats per bar
    set_instrument(melody, 73);        // flute
    set_instrument(accomp, 24);        // nylon guitar
    set_pan(melody, 80);
    set_pan(accomp, 48);
    set_volume(accomp, 85);

    const int root = 60;               // C major

    for (int barNo = 0; barNo < 8; barNo = barNo + 1) {
        if (barNo == 7) {
            set_tempo(100);            // ritardando into the final bar
            velocity = 112;
        }
        int step = root + 12 - barNo;  // melody walks down the scale

        // Beat 1: melody note together with the bass "oom" (sync),
        // beats 2 and 3: two sequential chord "pahs" while the melody rests.
        sync { play(melody, step, 1.0); play(accomp, root - 12, 1.0); }
        play(accomp, root + 4, 1.0);
        play(accomp, root + 7, 1.0);
    }

    sync {                              // final chord
        play(melody, root + 12, 3.0);
        play(accomp, root, 3.0);
        play(accomp, root + 4, 3.0);
        play(accomp, root + 7, 3.0);
    }
}
```

Compile and listen:

```bash
mipasm waltz.mip   # writes waltz.mid
```

The repository's `examples/` directory has three more: `happyBirthday.mip`
(minimal), `tetris.mip` (two-voice), and `ode-to-joy.mip` (every feature).

## 10. When things go wrong

The compiler runs four phases; each reports errors in its own voice and any
error stops the build (non-zero exit, no MIDI file):

| Phase | Catches | Example message |
|---|---|---|
| Lexical | unknown characters | `Unknown lexeme ...` |
| Syntactic | grammar violations | `syntax error, unexpected ...` (e.g. a `set_tempo` outside `main`, a missing `;`) |
| Semantic | type and declaration errors | `Track 'ghost' is not declared.` / `Cannot assign to const 'tempo_intro'.` |
| Generation | value errors found while "running" the program | `set_tempo requires a positive BPM, got 0.` / `For-loop exceeded iteration limit 100000.` |

Values that are merely *out of range* but harmless (a note above 127, a CC
value of 300, an instrument program of -2) do not fail the build: the compiler
clamps them into range and prints a warning.

When the output sounds wrong rather than failing, recompile with
`LOGGING_LEVEL=DEBUGGING`: the `Events:` dump lists every note with its start
beat (`t=...`), track, and duration, which usually makes timing bugs obvious.

## 11. Limits and gotchas

- **No user-defined functions, arrays, or strings** (yet) — `main()` is the
  only function; repetition is expressed with loops.
- **One time cursor.** `play(a, ...); play(b, ...);` is sequential even though
  the tracks differ. Parallel voices = `sync`.
- **`velocity` is the only reserved variable.** `const int tempo = ...` is just
  a normal variable and does *not* set the tempo — call `set_tempo()`.
- Assignment is not an expression: `while ((x = next) != 0)` does not parse.
- Integer division truncates; write `1 / 2.0` when you mean half a beat, or use
  a `duration`/`float` variable.
- A note's duration must end up positive; zero/negative durations are bumped to
  the minimum tick so the file stays well-formed.
- Loops cap at 100,000 iterations per loop as an infinite-loop guard.

## 12. Reference

- Statement-by-statement reference, MIDI byte encodings, and the compiler
  internals: [LANGUAGE_FEATURES.md](LANGUAGE_FEATURES.md)
- Grammar: `src/main/c/frontend/syntactic-analysis/BisonGrammar.y`
- Example programs: `examples/`
- Test programs (small, single-feature): `test/c/accept/` and `test/c/reject/`
