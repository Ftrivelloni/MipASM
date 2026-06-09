# MipASM Cookie for AI Agents

Use this file when generating or editing MipASM `.mip` programs. MipASM is a
small C-like language that compiles to MIDI. Prefer simple, explicit programs
that pass the compiler's syntax, semantic, and generation phases.

## Program Shape

A valid program has optional includes, optional global declarations or track
initializers, and exactly one `void main() { ... }`.

```mip
#include "<stdlib/audio>"

const int tempo = 120;
int velocity = 100;

track lead = init_track(0);

void main() {
    play(lead, 60, 1.0);
}
```

Includes must use the quoted angle-path form:

```mip
#include "<stdlib/audio>"
```

Comments are allowed:

```mip
// line comment
/* block comment */
```

## Types and Values

Supported value types:

- `int`: integer numbers, useful for MIDI notes, channels, loop counters, and
  velocities.
- `float`: decimal numbers.
- `boolean`: accepts boolean-compatible integer expressions; `0` is false and
  nonzero values are true.
- `duration`: beat lengths. Integers and floats can initialize or assign to
  durations.
- `track`: MIDI track handles. Do not declare a track with normal declaration
  syntax; create it with `track name = init_track(expression);`.

Valid declarations:

```mip
const int CHANNEL = 0;
float ratio = 0.5;
boolean enabled = 1;
duration beat = 1.0;
int note;
track lead = init_track(CHANNEL);
```

Invalid track declaration pattern:

```mip
track lead = 0; // invalid
```

Use initialized values before playback, loop bounds, track channels, and
durations. The compiler rejects programs that use uninitialized runtime values
in generation-critical places.

## Statements

Every declaration, assignment, domain call, and track initializer ends with a
semicolon.

Assignments:

```mip
note = note + 1;
velocity = 112;
```

Do not assign to `const` variables.

Control flow:

```mip
if (velocity > 100) {
    play(lead, 72, 0.5);
} else {
    play(lead, 60, 0.5);
}

for (int i = 0; i < 4; i = i + 1) {
    play(lead, 60 + i, 0.25);
}

int j = 0;
while (j < 4) {
    play(lead, 67, 0.25);
    j = j + 1;
}
```

Loop conditions must be boolean-compatible. Keep loops finite; generation has a
hard iteration limit and rejects runaway loops.

## Music Operations

Create tracks with numeric MIDI channels:

```mip
track lead = init_track(0);
track bass = init_track(1);
```

Play and rest:

```mip
play(lead, 60, 1.0);  // track, MIDI note number, duration in beats
rest(lead, 0.5);      // track, duration in beats
```

Controller statements:

```mip
set_volume(lead, 100);
set_pan(lead, 64);
set_attack(lead, 20);
```

The first argument to `play`, `rest`, and controller statements must be a
declared `track`, not an `int` or other value.

Special globals used by generation:

- `tempo`: if declared and initialized to a numeric value, sets the MIDI tempo.
- `velocity`: if declared and initialized/assigned to a numeric value, controls
  note velocity; otherwise notes default to velocity `100`.

## Expressions

Supported operators:

- Arithmetic: `+`, `-`, `*`, `/`
- Relational: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`
- Parentheses: `(expression)`

Operator precedence follows the grammar: multiplication/division bind tighter
than addition/subtraction; logical `&&` binds tighter than `||`; assignment is
right-associative but only appears in assignment statements and `for` headers.

Numeric expressions can combine `int`, `float`, and `duration` where the type
rules permit it. Track values are not numeric and cannot be assigned into `int`,
`float`, `boolean`, or `duration` variables.

## Sync Blocks

`sync { ... }` starts each contained statement at the same current time and then
advances time to the longest contained statement. Use it for chords or parallel
parts.

```mip
track lead = init_track(0);
track bass = init_track(1);

void main() {
    sync {
        play(lead, 64, 1.0);
        play(bass, 40, 1.0);
    }
    rest(lead, 0.5);
}
```

## Agent Checklist

Before returning a generated `.mip` program:

- Include `void main() { ... }`.
- Declare every variable before use.
- Initialize values that affect playback, loop bounds, track channels, and
  durations.
- Create tracks only with `track name = init_track(expr);`.
- Pass tracks, not channel integers, to `play`, `rest`, `set_volume`, `set_pan`,
  and `set_attack`.
- Separate call arguments with commas.
- End statements with semicolons.
- Keep loops finite and easy to evaluate.
- Avoid assigning to `const`.
- Use `#include "<path>"` if includes are needed.

## Good Minimal Examples

Single melody:

```mip
#include "<stdlib/audio>"

const int tempo = 120;
int velocity = 96;
track lead = init_track(0);

void main() {
    play(lead, 60, 1.0);
    play(lead, 62, 1.0);
    play(lead, 64, 1.0);
    rest(lead, 0.5);
}
```

Looped phrase with harmony:

```mip
const int tempo = 100;
int velocity = 100;

track lead = init_track(0);
track bass = init_track(1);

void main() {
    set_volume(lead, 110);
    set_volume(bass, 90);
    set_pan(lead, 80);
    set_pan(bass, 48);

    for (int i = 0; i < 4; i = i + 1) {
        sync {
            play(lead, 60 + i, 0.5);
            play(bass, 36, 0.5);
        }
    }
}
```

## Validation Commands

Build the compiler:

```bash
src/main/bash/build.sh
```

Run the compiler test corpus:

```bash
src/main/bash/test.sh
```

Compile an example to MIDI:

```bash
src/main/bash/run.sh examples/ode-to-joy.mip -o output.mid
```

