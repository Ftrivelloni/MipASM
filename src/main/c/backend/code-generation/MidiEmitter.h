#ifndef MIDI_EMITTER_HEADER
#define MIDI_EMITTER_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdbool.h>

/**
 * MidiEmitter is a self-contained backend module that turns a structured,
 * timed musical event stream into a Standard MIDI File (SMF, format 1).
 *
 * It owns the event model below and exposes a small builder API so the code
 * generator can populate a MusicProgram without knowing anything about the
 * MIDI byte format. The generator interprets the AST and, for every domain
 * statement (track init / play / rest / CC / tempo / time signature /
 * instrument), calls the matching musicProgram* builder; once interpretation
 * succeeds it calls emitMidiFile().
 *
 * This header depends only on the support layer (Logger, CompilationStatus,
 * ModuleDestructor) and libc, NOT on the AST, so it stays decoupled from the
 * frontend.
 */

/** The kind of a high-level musical event produced by the interpreter. */
typedef enum {
	MUSIC_EVENT_TRACK_INIT,		/* a `track x = init_track(ch)` declaration */
	MUSIC_EVENT_NOTE,			/* a `play(track, note, duration)` statement */
	MUSIC_EVENT_REST,			/* a `rest(track, duration)` statement     */
	MUSIC_EVENT_CC,				/* a set_volume / set_pan / ... statement  */
	MUSIC_EVENT_TEMPO,			/* a set_tempo(bpm); conductor-track event */
	MUSIC_EVENT_TIME_SIGNATURE,	/* a set_time_signature(n, d); conductor-track event */
	MUSIC_EVENT_PROGRAM_CHANGE	/* a set_instrument(track, program)        */
} MusicEventKind;

/**
 * The control-change family. The ordering intentionally mirrors the AST's
 * CCKind enum so the generator can translate with a plain cast, but the two
 * enums stay independent so this module never includes the AST header.
 *   MIDI_CC_VOLUME     -> controller 7  (Channel Volume)
 *   MIDI_CC_PAN        -> controller 10 (Pan)
 *   MIDI_CC_ATTACK     -> controller 73 (Sound Controller 4 / Attack Time)
 *   MIDI_CC_SUSTAIN    -> controller 64 (Sustain / Damper Pedal)
 *   MIDI_CC_MODULATION -> controller 1  (Modulation Wheel)
 *   MIDI_CC_REVERB     -> controller 91 (Effects 1 Depth / Reverb Send)
 */
typedef enum {
	MIDI_CC_VOLUME,
	MIDI_CC_PAN,
	MIDI_CC_ATTACK,
	MIDI_CC_SUSTAIN,
	MIDI_CC_MODULATION,
	MIDI_CC_REVERB
} MidiCCKind;

/**
 * A single timed musical event. Times are absolute and expressed in beat
 * units (one beat == one quarter note); the emitter converts them to MIDI
 * ticks. `trackName` is a borrowed pointer owned by the caller (typically an
 * AST string that outlives emission); this module never frees it.
 */
typedef struct MusicEvent {
	MusicEventKind kind;
	double beat;				/* absolute start time, in beats           */
	int channel;				/* MIDI channel 0..15                      */
	const char * trackName;		/* borrowed; groups events into MTrk chunks */

	/* NOTE / REST */
	int note;					/* MIDI note number 0..127                 */
	int velocity;				/* note-on velocity 1..127                 */
	double durationBeats;		/* note / rest length in beats             */

	/* CC */
	MidiCCKind ccKind;
	int ccValue;				/* rounded integer value 0..127            */
	bool ccValueWasFloat;		/* keeps the debug print byte-identical    */
	double ccValueFloat;		/* original float value, for the debug print */

	/* TEMPO (conductor track: trackName == NULL) */
	int tempoBPM;				/* beats per minute, > 0                   */

	/* TIME_SIGNATURE (conductor track: trackName == NULL) */
	int tsNumerator;			/* beats per bar, 1..255                   */
	int tsDenominator;			/* beat unit, a power of two in 1..32      */

	/* PROGRAM_CHANGE */
	int programNumber;			/* General MIDI program 0..127             */

	struct MusicEvent * next;
} MusicEvent;

/** Declaration-order record of one MIDI track (one per `init_track`). */
typedef struct MidiTrackInfo {
	const char * name;			/* borrowed                                */
	int channel;
	struct MidiTrackInfo * next;
} MidiTrackInfo;

/**
 * The complete structured program handed from the generator to the emitter.
 * Heap-allocated by createMusicProgram() so it can survive past the
 * interpreter and be consumed by emitMidiFile(), then released with
 * destroyMusicProgram().
 */
typedef struct {
	MusicEvent * eventsHead;
	MusicEvent * eventsTail;
	MidiTrackInfo * tracksHead;
	MidiTrackInfo * tracksTail;
	int trackCount;
} MusicProgram;

/** Initialize module's internal state. */
ModuleDestructor initializeMidiEmitterModule(void);

/* BUILDER API (called by the code generator) */

/** Allocates an empty program. */
MusicProgram * createMusicProgram(void);

/** Records a track declaration and creates its MTrk chunk slot. */
void musicProgramAddTrack(MusicProgram * program, const char * name, int channel);

/** Records a tempo change (FF 51 in the conductor track). `bpm` must be positive. */
void musicProgramAddTempo(MusicProgram * program, double beat, int bpm);

/** Records a time-signature change (FF 58 in the conductor track). */
void musicProgramAddTimeSignature(MusicProgram * program, double beat, int numerator, int denominator);

/** Records an instrument selection (Program Change) for a track. */
void musicProgramAddProgramChange(MusicProgram * program, double beat, int channel, const char * track, int programNumber);

/** Records a played note (start beat, channel, note, velocity, length). */
void musicProgramAddNote(MusicProgram * program, double beat, int channel, const char * track, int note, int velocity, double durationBeats);

/** Records a rest (time-only; produces no MIDI bytes, kept for the debug print). */
void musicProgramAddRest(MusicProgram * program, double beat, const char * track, double durationBeats);

/** Records a control-change event. `wasFloat`/`floatValue` only affect the debug print. */
void musicProgramAddCC(MusicProgram * program, double beat, int channel, const char * track, MidiCCKind kind, int value, bool wasFloat, double floatValue);

/** Frees the program, its events and its track list (borrowed names are NOT freed). */
void destroyMusicProgram(MusicProgram * program);

/**
 * Reproduces the generator's textual `Events:` stdout dump from the structured
 * program. Optional: lets the generator keep its debug output byte-identical
 * after switching to the structured model.
 */
void printMusicProgram(const MusicProgram * program);

/**
 * Writes a Standard MIDI File (format 1) describing `program` to `outputPath`.
 * Returns SUCCEEDED on success, FAILED if the file cannot be written.
 */
CompilationStatus emitMidiFile(const MusicProgram * program, const char * outputPath);

#endif
