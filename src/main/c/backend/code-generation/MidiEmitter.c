#include "MidiEmitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/* MIDI constants. */
#define MIDI_PPQ 480				/* ticks per quarter note (the SMF division) */
#define MIDI_DEFAULT_TEMPO_BPM 120	/* forgot to put global tempo variable apparently, temporary fix     */
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
	MusicProgram * program = (MusicProgram *) calloc(1, sizeof(MusicProgram));
	program->tempoBPM = MIDI_DEFAULT_TEMPO_BPM;
	return program;
}

void musicProgramSetTempo(MusicProgram * program, int bpm) {
	if (program == NULL) {
		return;
	}
	if (bpm > 0) {
		program->tempoBPM = bpm;
	} else {
		logWarning(_logger, "Ignoring non-positive tempo %d; keeping %d BPM.", bpm, program->tempoBPM);
	}
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
	int order;					/* 0 = note-off / CC, 1 = note-on (off sorts first at equal tick) */
	unsigned long seq;			/* insertion index: stable tie-break for qsort */
	unsigned char status;
	unsigned char data1;
	unsigned char data2;
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

/** Write the conductor track: a single tempo meta event plus End-of-Track. */
static void _writeConductorChunk(FILE * file, int tempoBPM) {
	int bpm = tempoBPM > 0 ? tempoBPM : MIDI_DEFAULT_TEMPO_BPM;
	unsigned long microsecondsPerQuarter = MICROSECONDS_PER_MINUTE / (unsigned long) bpm;

	ByteBuffer body;
	_bufferInit(&body);
	/* delta 0, FF 51 03, then the 24-bit tempo value. */
	_bufferPutVLQ(&body, 0);
	_bufferPutU8(&body, 0xFF);
	_bufferPutU8(&body, 0x51);
	_bufferPutU8(&body, 0x03);
	_bufferPutU8(&body, (unsigned int) ((microsecondsPerQuarter >> 16) & 0xFF));
	_bufferPutU8(&body, (unsigned int) ((microsecondsPerQuarter >> 8) & 0xFF));
	_bufferPutU8(&body, (unsigned int) (microsecondsPerQuarter & 0xFF));
	/* delta 0, End of Track (FF 2F 00). */
	_bufferPutVLQ(&body, 0);
	_bufferPutU8(&body, 0xFF);
	_bufferPutU8(&body, 0x2F);
	_bufferPutU8(&body, 0x00);

	fwrite("MTrk", 1, 4, file);
	_writeU32BE(file, (unsigned int) body.length);
	fwrite(body.data, 1, body.length, file);
	_bufferFree(&body);
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
		} else if (event->kind == MUSIC_EVENT_CC) {
			count += 1;
		}
	}
	return count;
}

/** Build, sort, delta-encode and write one MTrk chunk for a declared track. */
static void _writeTrackChunk(FILE * file, const MusicProgram * program, const MidiTrackInfo * track) {
	unsigned char channel = (unsigned char) (track->channel & 0x0F);
	if (track->channel < 0 || track->channel > 15) {
		logWarning(_logger, "Track '%s' channel %d out of range 0..15; using %d.", track->name, track->channel, (int) channel);
	}

	size_t rawCount = _countRawEvents(program, track->name);
	RawMidiEvent * raw = NULL;
	if (rawCount > 0) {
		raw = (RawMidiEvent *) calloc(rawCount, sizeof(RawMidiEvent));
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
			index++;
			/* note-off */
			raw[index].tick = offTick;
			raw[index].order = 0;
			raw[index].seq = sequence++;
			raw[index].status = (unsigned char) (0x80 | channel);
			raw[index].data1 = note;
			raw[index].data2 = 0;
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
			index++;
		}
	}

	if (rawCount > 0) {
		qsort(raw, rawCount, sizeof(RawMidiEvent), _compareRawEvents);
	}

	ByteBuffer body;
	_bufferInit(&body);
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
		_bufferPutU8(&body, raw[i].data2);
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

	_writeConductorChunk(file, program->tempoBPM);
	for (MidiTrackInfo * track = program->tracksHead; track != NULL; track = track->next) {
		_writeTrackChunk(file, program, track);
	}

	fclose(file);
	logDebugging(_logger, "Wrote MIDI file '%s' (%u track chunks, %d BPM).", path, trackChunks, program->tempoBPM);
	return SUCCEEDED;
}
