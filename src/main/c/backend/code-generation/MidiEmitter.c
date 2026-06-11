#include "MidiEmitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/* MIDI constants. */
#define MIDI_PPQ 480				/* ticks per quarter note (the SMF division) */
#define MIDI_DEFAULT_TEMPO_BPM 120	/* used when the program never calls set_tempo */
#define MICROSECONDS_PER_MINUTE 60000000UL

/** Shutdown module's internal state. */
void _shutdownMidiEmitterModule(void) {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: MidiEmitter...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

/* PUBLIC FUNCTIONS */

ModuleDestructor initializeMidiEmitterModule(void) {
	_logger = createLogger("MidiEmitter");
	return _shutdownMidiEmitterModule;
}

/* BUILDER API */

MusicProgram * createMusicProgram(void) {
	return (MusicProgram *) calloc(1, sizeof(MusicProgram));
}

/** Allocates a zeroed event of the given kind and appends it to the program. */
static MusicEvent * _appendEvent(MusicProgram * program, MusicEventKind kind) {
	MusicEvent * event = (MusicEvent *) calloc(1, sizeof(MusicEvent));
	event->kind = kind;
	if (program->eventsTail == NULL) {
		program->eventsHead = event;
		program->eventsTail = event;
	} else {
		program->eventsTail->next = event;
		program->eventsTail = event;
	}
	return event;
}

void musicProgramAddTrack(MusicProgram * program, const char * name, int channel) {
	if (program == NULL) {
		return;
	}
	MusicEvent * event = _appendEvent(program, MUSIC_EVENT_TRACK_INIT);
	event->trackName = name;
	event->channel = channel;

	MidiTrackInfo * info = (MidiTrackInfo *) calloc(1, sizeof(MidiTrackInfo));
	info->name = name;
	info->channel = channel;
	if (program->tracksTail == NULL) {
		program->tracksHead = info;
		program->tracksTail = info;
	} else {
		program->tracksTail->next = info;
		program->tracksTail = info;
	}
	program->trackCount++;
}

void musicProgramAddNote(MusicProgram * program, double beat, int channel, const char * track, int note, int velocity, double durationBeats) {
	if (program == NULL) {
		return;
	}
	MusicEvent * event = _appendEvent(program, MUSIC_EVENT_NOTE);
	event->beat = beat;
	event->channel = channel;
	event->trackName = track;
	event->note = note;
	event->velocity = velocity;
	event->durationBeats = durationBeats;
}

void musicProgramAddRest(MusicProgram * program, double beat, const char * track, double durationBeats) {
	if (program == NULL) {
		return;
	}
	MusicEvent * event = _appendEvent(program, MUSIC_EVENT_REST);
	event->beat = beat;
	event->trackName = track;
	event->durationBeats = durationBeats;
}

void musicProgramAddCC(MusicProgram * program, double beat, int channel, const char * track, MidiCCKind kind, int value, bool wasFloat, double floatValue) {
	if (program == NULL) {
		return;
	}
	MusicEvent * event = _appendEvent(program, MUSIC_EVENT_CC);
	event->beat = beat;
	event->channel = channel;
	event->trackName = track;
	event->ccKind = kind;
	event->ccValue = value;
	event->ccValueWasFloat = wasFloat;
	event->ccValueFloat = floatValue;
}

void musicProgramAddTempo(MusicProgram * program, double beat, int bpm) {
	if (program == NULL) {
		return;
	}
	MusicEvent * event = _appendEvent(program, MUSIC_EVENT_TEMPO);
	event->beat = beat;
	event->tempoBPM = bpm;	/* trackName stays NULL: conductor-track event */
}

void musicProgramAddTimeSignature(MusicProgram * program, double beat, int numerator, int denominator) {
	if (program == NULL) {
		return;
	}
	MusicEvent * event = _appendEvent(program, MUSIC_EVENT_TIME_SIGNATURE);
	event->beat = beat;
	event->tsNumerator = numerator;	/* trackName stays NULL: conductor-track event */
	event->tsDenominator = denominator;
}

void musicProgramAddProgramChange(MusicProgram * program, double beat, int channel, const char * track, int programNumber) {
	if (program == NULL) {
		return;
	}
	MusicEvent * event = _appendEvent(program, MUSIC_EVENT_PROGRAM_CHANGE);
	event->beat = beat;
	event->channel = channel;
	event->trackName = track;
	event->programNumber = programNumber;
}

void destroyMusicProgram(MusicProgram * program) {
	if (program == NULL) {
		return;
	}
	MusicEvent * event = program->eventsHead;
	while (event != NULL) {
		MusicEvent * next = event->next;
		free(event);
		event = next;
	}
	MidiTrackInfo * track = program->tracksHead;
	while (track != NULL) {
		MidiTrackInfo * next = track->next;
		free(track);			/* names are borrowed; do not free them */
		track = next;
	}
	free(program);
}

/* TEXTUAL DEBUG DUMP (mirrors the generator's original `Events:` output) */

static const char * _ccLabel(MidiCCKind kind) {
	switch (kind) {
		case MIDI_CC_PAN: return "pan";
		case MIDI_CC_ATTACK: return "attack";
		case MIDI_CC_SUSTAIN: return "sustain";
		case MIDI_CC_MODULATION: return "modulation";
		case MIDI_CC_REVERB: return "reverb";
		case MIDI_CC_VOLUME:
		default: return "volume";
	}
}

void printMusicProgram(const MusicProgram * program) {
	printf("Events:\n");
	if (program == NULL) {
		return;
	}
	for (MusicEvent * event = program->eventsHead; event != NULL; event = event->next) {
		switch (event->kind) {
			case MUSIC_EVENT_TRACK_INIT:
				printf("track %s channel=%d\n", event->trackName, event->channel);
				break;
			case MUSIC_EVENT_NOTE:
				printf("t=%.3f play %s note=%d duration=%.3f\n",
					event->beat, event->trackName, event->note, event->durationBeats);
				break;
			case MUSIC_EVENT_REST:
				printf("t=%.3f rest %s duration=%.3f\n",
					event->beat, event->trackName, event->durationBeats);
				break;
			case MUSIC_EVENT_CC:
				if (event->ccValueWasFloat) {
					printf("t=%.3f cc %s %s=%.3f\n",
						event->beat, event->trackName, _ccLabel(event->ccKind), event->ccValueFloat);
				} else {
					printf("t=%.3f cc %s %s=%d\n",
						event->beat, event->trackName, _ccLabel(event->ccKind), event->ccValue);
				}
				break;
			case MUSIC_EVENT_TEMPO:
				printf("t=%.3f tempo %d\n", event->beat, event->tempoBPM);
				break;
			case MUSIC_EVENT_TIME_SIGNATURE:
				printf("t=%.3f time_signature %d/%d\n",
					event->beat, event->tsNumerator, event->tsDenominator);
				break;
			case MUSIC_EVENT_PROGRAM_CHANGE:
				printf("t=%.3f program %s value=%d\n",
					event->beat, event->trackName, event->programNumber);
				break;
		}
	}
}

/* MIDI BYTE ENCODING */

/** Clamp to the 7-bit range used by note numbers and CC values. */
static unsigned char _clamp7(int value) {
	if (value < 0) return 0;
	if (value > 127) return 127;
	return (unsigned char) value;
}

/** Clamp a note-on velocity to 1..127 (a velocity of 0 would mean note-off). */
static unsigned char _clampVelocity(int value) {
	if (value < 1) return 1;
	if (value > 127) return 127;
	return (unsigned char) value;
}

/** Translate a high-level CC kind to its MIDI controller number. */
static unsigned char _controllerNumber(MidiCCKind kind) {
	switch (kind) {
		case MIDI_CC_VOLUME: return 7;
		case MIDI_CC_PAN: return 10;
		case MIDI_CC_ATTACK: return 73;
		case MIDI_CC_SUSTAIN: return 64;
		case MIDI_CC_MODULATION: return 1;
		case MIDI_CC_REVERB: return 91;
		default: return 7;
	}
}

/** Convert an absolute beat position to absolute MIDI ticks (round to nearest). */
static long _beatToTick(double beat) {
	if (beat < 0.0) {
		beat = 0.0;
	}
	return (long) (beat * (double) MIDI_PPQ + 0.5);
}

/* Big-endian fixed-width writers (all SMF multi-byte fields are big-endian). */

static void _writeU16BE(FILE * file, unsigned int value) {
	fputc((int) ((value >> 8) & 0xFF), file);
	fputc((int) (value & 0xFF), file);
}

static void _writeU32BE(FILE * file, unsigned int value) {
	fputc((int) ((value >> 24) & 0xFF), file);
	fputc((int) ((value >> 16) & 0xFF), file);
	fputc((int) ((value >> 8) & 0xFF), file);
	fputc((int) (value & 0xFF), file);
}

/* A small growable byte buffer used to assemble an MTrk body before we know
 * its length (the MTrk chunk is length-prefixed). Buffering avoids fseek
 * back-patching, so emission also works when the output is a pipe. */

typedef struct {
	unsigned char * data;
	size_t length;
	size_t capacity;
} ByteBuffer;

static void _bufferInit(ByteBuffer * buffer) {
	buffer->capacity = 64;
	buffer->length = 0;
	buffer->data = (unsigned char *) malloc(buffer->capacity);
}

static void _bufferEnsure(ByteBuffer * buffer, size_t extra) {
	if (buffer->length + extra <= buffer->capacity) {
		return;
	}
	while (buffer->length + extra > buffer->capacity) {
		buffer->capacity *= 2;
	}
	buffer->data = (unsigned char *) realloc(buffer->data, buffer->capacity);
}

static void _bufferPutU8(ByteBuffer * buffer, unsigned int value) {
	_bufferEnsure(buffer, 1);
	buffer->data[buffer->length++] = (unsigned char) (value & 0xFF);
}

/** Append a MIDI variable-length quantity (7 bits per byte, MSB = continuation). */
static void _bufferPutVLQ(ByteBuffer * buffer, unsigned long value) {
	unsigned char chunk[5];
	int count = 0;
	chunk[count++] = (unsigned char) (value & 0x7F);
	value >>= 7;
	while (value > 0) {
		chunk[count++] = (unsigned char) ((value & 0x7F) | 0x80);
		value >>= 7;
	}
	/* `chunk` holds the VLQ least-significant-group first; emit most first. */
	for (int i = count - 1; i >= 0; --i) {
		_bufferPutU8(buffer, chunk[i]);
	}
}

static void _bufferFree(ByteBuffer * buffer) {
	free(buffer->data);
	buffer->data = NULL;
	buffer->length = 0;
	buffer->capacity = 0;
}

/* A raw, channel-level MIDI event with an absolute tick, ready to be sorted
 * and delta-encoded. `order` and `seq` give a deterministic stable sort. */

typedef struct {
	long tick;
	int order;					/* 0 = note-off / CC / program change, 1 = note-on (off sorts first at equal tick) */
	unsigned long seq;			/* insertion index: stable tie-break for qsort */
	unsigned char status;
	unsigned char data1;
	unsigned char data2;
	int dataLength;				/* data bytes after status: 2 (notes, CC) or 1 (program change) */
} RawMidiEvent;

static int _compareRawEvents(const void * left, const void * right) {
	const RawMidiEvent * a = (const RawMidiEvent *) left;
	const RawMidiEvent * b = (const RawMidiEvent *) right;
	if (a->tick != b->tick) {
		return a->tick < b->tick ? -1 : 1;
	}
	if (a->order != b->order) {
		return a->order < b->order ? -1 : 1;
	}
	if (a->seq != b->seq) {
		return a->seq < b->seq ? -1 : 1;
	}
	return 0;
}

/** Append a Track Name meta event (delta 0, FF 03, length, name bytes). */
static void _bufferPutTrackName(ByteBuffer * buffer, const char * name) {
	if (name == NULL) {
		return;
	}
	size_t length = strlen(name);
	_bufferPutVLQ(buffer, 0);
	_bufferPutU8(buffer, 0xFF);
	_bufferPutU8(buffer, 0x03);
	_bufferPutVLQ(buffer, (unsigned long) length);
	for (size_t i = 0; i < length; ++i) {
		_bufferPutU8(buffer, (unsigned int) (unsigned char) name[i]);
	}
}

/** Append a tempo meta event (FF 51 03 + 24-bit microseconds per quarter note). */
static void _bufferPutTempo(ByteBuffer * buffer, int bpm) {
	unsigned long microsecondsPerQuarter = MICROSECONDS_PER_MINUTE / (unsigned long) bpm;
	_bufferPutU8(buffer, 0xFF);
	_bufferPutU8(buffer, 0x51);
	_bufferPutU8(buffer, 0x03);
	_bufferPutU8(buffer, (unsigned int) ((microsecondsPerQuarter >> 16) & 0xFF));
	_bufferPutU8(buffer, (unsigned int) ((microsecondsPerQuarter >> 8) & 0xFF));
	_bufferPutU8(buffer, (unsigned int) (microsecondsPerQuarter & 0xFF));
}

/* A conductor-track meta event (tempo or time signature) with an absolute
 * tick. Like RawMidiEvent, `seq` keeps the sort stable. */

typedef struct {
	long tick;
	unsigned long seq;
	MusicEventKind kind;
	int a;						/* tempo BPM, or time-signature numerator   */
	int b;						/* time-signature denominator (unused for tempo) */
} ConductorEvent;

static int _compareConductorEvents(const void * left, const void * right) {
	const ConductorEvent * x = (const ConductorEvent *) left;
	const ConductorEvent * y = (const ConductorEvent *) right;
	if (x->tick != y->tick) {
		return x->tick < y->tick ? -1 : 1;
	}
	if (x->seq != y->seq) {
		return x->seq < y->seq ? -1 : 1;
	}
	return 0;
}

/** log2 of a power of two in 1..32 (validated by the generator). */
static unsigned char _denominatorPower(int denominator) {
	unsigned char power = 0;
	while (denominator > 1) {
		denominator >>= 1;
		power++;
	}
	return power;
}

/**
 * Write the conductor track: every tempo / time-signature event sorted by
 * tick and delta-encoded, plus End-of-Track. If the program never sets a
 * tempo at tick 0, a default 120 BPM event is emitted first so playback
 * timing is explicit.
 */
static bool _writeConductorChunk(FILE * file, const MusicProgram * program) {
	size_t count = 0;
	for (MusicEvent * event = program->eventsHead; event != NULL; event = event->next) {
		if (event->kind == MUSIC_EVENT_TEMPO || event->kind == MUSIC_EVENT_TIME_SIGNATURE) {
			count++;
		}
	}

	ConductorEvent * events = NULL;
	if (count > 0) {
		events = (ConductorEvent *) calloc(count, sizeof(ConductorEvent));
		if (events == NULL) {
			logError(_logger, "Cannot allocate %zu conductor event(s).", count);
			return false;
		}
	}

	size_t index = 0;
	unsigned long sequence = 0;
	bool hasInitialTempo = false;
	for (MusicEvent * event = program->eventsHead; event != NULL; event = event->next) {
		if (event->kind != MUSIC_EVENT_TEMPO && event->kind != MUSIC_EVENT_TIME_SIGNATURE) {
			continue;
		}
		events[index].tick = _beatToTick(event->beat);
		events[index].seq = sequence++;
		events[index].kind = event->kind;
		if (event->kind == MUSIC_EVENT_TEMPO) {
			events[index].a = event->tempoBPM;
			if (events[index].tick == 0) {
				hasInitialTempo = true;
			}
		} else {
			events[index].a = event->tsNumerator;
			events[index].b = event->tsDenominator;
		}
		index++;
	}

	if (count > 0) {
		qsort(events, count, sizeof(ConductorEvent), _compareConductorEvents);
	}

	ByteBuffer body;
	_bufferInit(&body);
	_bufferPutTrackName(&body, "conductor");
	if (!hasInitialTempo) {
		_bufferPutVLQ(&body, 0);
		_bufferPutTempo(&body, MIDI_DEFAULT_TEMPO_BPM);
	}
	long previousTick = 0;
	for (size_t i = 0; i < count; ++i) {
		long delta = events[i].tick - previousTick;
		previousTick = events[i].tick;
		if (delta < 0) {
			delta = 0;
		}
		_bufferPutVLQ(&body, (unsigned long) delta);
		if (events[i].kind == MUSIC_EVENT_TEMPO) {
			_bufferPutTempo(&body, events[i].a);
		} else {
			/* FF 58 04 nn dd cc bb: cc = 24 MIDI clocks per metronome click,
			 * bb = 8 thirty-second notes per quarter (the SMF defaults). */
			_bufferPutU8(&body, 0xFF);
			_bufferPutU8(&body, 0x58);
			_bufferPutU8(&body, 0x04);
			_bufferPutU8(&body, (unsigned int) (events[i].a & 0xFF));
			_bufferPutU8(&body, _denominatorPower(events[i].b));
			_bufferPutU8(&body, 0x18);
			_bufferPutU8(&body, 0x08);
		}
	}
	/* delta 0, End of Track (FF 2F 00). */
	_bufferPutVLQ(&body, 0);
	_bufferPutU8(&body, 0xFF);
	_bufferPutU8(&body, 0x2F);
	_bufferPutU8(&body, 0x00);

	fwrite("MTrk", 1, 4, file);
	_writeU32BE(file, (unsigned int) body.length);
	fwrite(body.data, 1, body.length, file);
	_bufferFree(&body);
	free(events);
	return true;
}

/** Count the raw MIDI events a single track contributes (notes = 2, CC = 1). */
static size_t _countRawEvents(const MusicProgram * program, const char * trackName) {
	size_t count = 0;
	for (MusicEvent * event = program->eventsHead; event != NULL; event = event->next) {
		if (event->trackName == NULL || strcmp(event->trackName, trackName) != 0) {
			continue;
		}
		if (event->kind == MUSIC_EVENT_NOTE) {
			count += 2;
		} else if (event->kind == MUSIC_EVENT_CC || event->kind == MUSIC_EVENT_PROGRAM_CHANGE) {
			count += 1;
		}
	}
	return count;
}

/** Build, sort, delta-encode and write one MTrk chunk for a declared track. */
static bool _writeTrackChunk(FILE * file, const MusicProgram * program, const MidiTrackInfo * track) {
	unsigned char channel = (unsigned char) (track->channel & 0x0F);
	if (track->channel < 0 || track->channel > 15) {
		logWarning(_logger, "Track '%s' channel %d out of range 0..15; using %d.", track->name, track->channel, (int) channel);
	}

	size_t rawCount = _countRawEvents(program, track->name);
	RawMidiEvent * raw = NULL;
	if (rawCount > 0) {
		raw = (RawMidiEvent *) calloc(rawCount, sizeof(RawMidiEvent));
		if (raw == NULL) {
			logError(_logger, "Cannot allocate %zu raw MIDI event(s) for track '%s'.", rawCount, track->name);
			return false;
		}
	}

	size_t index = 0;
	unsigned long sequence = 0;
	for (MusicEvent * event = program->eventsHead; event != NULL; event = event->next) {
		if (event->trackName == NULL || strcmp(event->trackName, track->name) != 0) {
			continue;
		}
		if (event->kind == MUSIC_EVENT_NOTE) {
			if (event->note < 0 || event->note > 127) {
				logWarning(_logger, "Note %d on '%s' out of range 0..127; clamping.", event->note, track->name);
			}
			unsigned char note = _clamp7(event->note);
			unsigned char velocity = _clampVelocity(event->velocity);
			long onTick = _beatToTick(event->beat);
			long offTick = _beatToTick(event->beat + event->durationBeats);
			if (offTick <= onTick) {
				offTick = onTick + 1;	/* guarantee an audible, well-ordered note */
			}
			/* note-on */
			raw[index].tick = onTick;
			raw[index].order = 1;
			raw[index].seq = sequence++;
			raw[index].status = (unsigned char) (0x90 | channel);
			raw[index].data1 = note;
			raw[index].data2 = velocity;
			raw[index].dataLength = 2;
			index++;
			/* note-off */
			raw[index].tick = offTick;
			raw[index].order = 0;
			raw[index].seq = sequence++;
			raw[index].status = (unsigned char) (0x80 | channel);
			raw[index].data1 = note;
			raw[index].data2 = 0;
			raw[index].dataLength = 2;
			index++;
		} else if (event->kind == MUSIC_EVENT_CC) {
			if (event->ccValue < 0 || event->ccValue > 127) {
				logWarning(_logger, "CC value %d on '%s' out of range 0..127; clamping.", event->ccValue, track->name);
			}
			raw[index].tick = _beatToTick(event->beat);
			raw[index].order = 0;	/* land before note-ons at the same tick */
			raw[index].seq = sequence++;
			raw[index].status = (unsigned char) (0xB0 | channel);
			raw[index].data1 = _controllerNumber(event->ccKind);
			raw[index].data2 = _clamp7(event->ccValue);
			raw[index].dataLength = 2;
			index++;
		} else if (event->kind == MUSIC_EVENT_PROGRAM_CHANGE) {
			if (event->programNumber < 0 || event->programNumber > 127) {
				logWarning(_logger, "Instrument program %d on '%s' out of range 0..127; clamping.", event->programNumber, track->name);
			}
			raw[index].tick = _beatToTick(event->beat);
			raw[index].order = 0;	/* land before note-ons at the same tick */
			raw[index].seq = sequence++;
			raw[index].status = (unsigned char) (0xC0 | channel);
			raw[index].data1 = _clamp7(event->programNumber);
			raw[index].dataLength = 1;
			index++;
		}
	}

	if (rawCount > 0) {
		qsort(raw, rawCount, sizeof(RawMidiEvent), _compareRawEvents);
	}

	ByteBuffer body;
	_bufferInit(&body);
	_bufferPutTrackName(&body, track->name);
	long previousTick = 0;
	for (size_t i = 0; i < rawCount; ++i) {
		long delta = raw[i].tick - previousTick;
		previousTick = raw[i].tick;
		if (delta < 0) {
			delta = 0;
		}
		_bufferPutVLQ(&body, (unsigned long) delta);
		_bufferPutU8(&body, raw[i].status);
		_bufferPutU8(&body, raw[i].data1);
		if (raw[i].dataLength == 2) {
			_bufferPutU8(&body, raw[i].data2);
		}
	}
	/* End of Track. */
	_bufferPutVLQ(&body, 0);
	_bufferPutU8(&body, 0xFF);
	_bufferPutU8(&body, 0x2F);
	_bufferPutU8(&body, 0x00);

	fwrite("MTrk", 1, 4, file);
	_writeU32BE(file, (unsigned int) body.length);
	fwrite(body.data, 1, body.length, file);

	_bufferFree(&body);
	free(raw);
	return true;
}

CompilationStatus emitMidiFile(const MusicProgram * program, const char * outputPath) {
	if (program == NULL) {
		logError(_logger, "Cannot emit a MIDI file from a NULL program.");
		return FAILED;
	}
	const char * path = (outputPath != NULL && outputPath[0] != '\0') ? outputPath : "output.mid";
	FILE * file = fopen(path, "wb");
	if (file == NULL) {
		logError(_logger, "Cannot open '%s' for writing.", path);
		return FAILED;
	}

	unsigned int trackChunks = 1u + (unsigned int) program->trackCount;	/* +1 conductor track */

	/* MThd: chunk length 6, format 1, ntracks, division (PPQ). */
	fwrite("MThd", 1, 4, file);
	_writeU32BE(file, 6);
	_writeU16BE(file, 1);
	_writeU16BE(file, trackChunks);
	_writeU16BE(file, MIDI_PPQ);

	if (!_writeConductorChunk(file, program)) {
		fclose(file);
		return FAILED;
	}
	for (MidiTrackInfo * track = program->tracksHead; track != NULL; track = track->next) {
		if (!_writeTrackChunk(file, program, track)) {
			fclose(file);
			return FAILED;
		}
	}

	fclose(file);
	logDebugging(_logger, "Wrote MIDI file '%s' (%u track chunks).", path, trackChunks);
	return SUCCEEDED;
}
