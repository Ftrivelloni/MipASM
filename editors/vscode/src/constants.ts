// MipASM language vocabulary used by the completion provider.
//
// The CONSTANT_GROUPS lists are the `const` names shipped in the compiler's
// standard library (lib/stdlib/*.mip). Regenerate them after changing the
// library with, from the repository root:
//
//   grep -oE '\bconst[[:space:]]+[a-zA-Z_]+[[:space:]]+[A-Za-z_][A-Za-z0-9_]*' \
//     lib/stdlib/<file>.mip | awk '{print $3}'

/** Type/storage keywords (`storage.type`). */
export const TYPES: string[] = [
  "void", "int", "float", "boolean", "track", "duration", "const"
];

/** Control-flow keywords (`keyword.control`). */
export const CONTROL_KEYWORDS: string[] = [
  "main", "if", "else", "for", "while", "sync"
];

/** Domain built-in statements, with an argument snippet for each. */
export interface Builtin {
  name: string;
  detail: string;
  snippet: string;
}

export const BUILTINS: Builtin[] = [
  { name: "init_track", detail: "track init_track(channel)", snippet: "init_track(${1:channel})" },
  { name: "play", detail: "play(track, note, duration)", snippet: "play(${1:track}, ${2:note}, ${3:duration});" },
  { name: "rest", detail: "rest(track, duration)", snippet: "rest(${1:track}, ${2:duration});" },
  { name: "set_volume", detail: "set_volume(track, 0..127)", snippet: "set_volume(${1:track}, ${2:value});" },
  { name: "set_pan", detail: "set_pan(track, 0..127)", snippet: "set_pan(${1:track}, ${2:value});" },
  { name: "set_attack", detail: "set_attack(track, 0..127)", snippet: "set_attack(${1:track}, ${2:value});" },
  { name: "set_sustain", detail: "set_sustain(track, 0..127)", snippet: "set_sustain(${1:track}, ${2:value});" },
  { name: "set_modulation", detail: "set_modulation(track, 0..127)", snippet: "set_modulation(${1:track}, ${2:value});" },
  { name: "set_reverb", detail: "set_reverb(track, 0..127)", snippet: "set_reverb(${1:track}, ${2:value});" },
  { name: "set_tempo", detail: "set_tempo(bpm)", snippet: "set_tempo(${1:120});" },
  { name: "set_instrument", detail: "set_instrument(track, program)", snippet: "set_instrument(${1:track}, ${2:program});" },
  { name: "set_time_signature", detail: "set_time_signature(numerator, denominator)", snippet: "set_time_signature(${1:4}, ${2:4});" }
];

/** Named constants from the standard library, grouped for nicer completion.
 *
 *  Each group is keyed by the stdlib `library` it lives in (the name used in
 *  `#include "<stdlib/...>"`). Constants are only offered when their library is
 *  imported by the current document — see STDLIB_DEPENDENCIES for the transitive
 *  includes that the standard library performs internally. */
export interface ConstantGroup {
  /** stdlib module these constants are defined in (lib/stdlib/<library>.mip). */
  library: string;
  detail: string;
  names: string[];
}

/** Includes that stdlib modules perform internally, so an explicit import of a
 *  module also brings these in. Keep in sync with the `#include` lines in
 *  lib/stdlib/*.mip (currently only chords pulls in scales). */
export const STDLIB_DEPENDENCIES: Record<string, string[]> = {
  chords: ["scales"]
};

export const CONSTANT_GROUPS: ConstantGroup[] = [
  {
    library: "audio",
    detail: "dynamic / duration / pan",
    names: [
      "PIANISSIMO", "PIANO", "MEZZO_PIANO", "MEZZO_FORTE", "FORTE", "FORTISSIMO",
      "WHOLE", "HALF", "QUARTER", "EIGHTH", "SIXTEENTH",
      "LEFT", "CENTER", "RIGHT"
    ]
  },
  {
    library: "notes",
    detail: "note",
    names: [
      "C0", "CS0", "D0", "DS0", "E0", "F0", "FS0", "G0", "GS0", "A0", "AS0", "B0",
      "C1", "CS1", "D1", "DS1", "E1", "F1", "FS1", "G1", "GS1", "A1", "AS1", "B1",
      "C2", "CS2", "D2", "DS2", "E2", "F2", "FS2", "G2", "GS2", "A2", "AS2", "B2",
      "C3", "CS3", "D3", "DS3", "E3", "F3", "FS3", "G3", "GS3", "A3", "AS3", "B3",
      "C4", "CS4", "D4", "DS4", "E4", "F4", "FS4", "G4", "GS4", "A4", "AS4", "B4",
      "C5", "CS5", "D5", "DS5", "E5", "F5", "FS5", "G5", "GS5", "A5", "AS5", "B5",
      "C6", "CS6", "D6", "DS6", "E6", "F6", "FS6", "G6", "GS6", "A6", "AS6", "B6",
      "C7", "CS7", "D7", "DS7", "E7", "F7", "FS7", "G7", "GS7", "A7", "AS7", "B7",
      "C8", "CS8", "D8", "DS8", "E8", "F8", "FS8", "G8", "GS8", "A8", "AS8", "B8"
    ]
  },
  {
    library: "instruments",
    detail: "instrument (GM program)",
    names: [
      "ACOUSTIC_GRAND_PIANO", "BRIGHT_ACOUSTIC_PIANO", "ELECTRIC_GRAND_PIANO", "HONKY_TONK_PIANO",
      "ELECTRIC_PIANO_1", "ELECTRIC_PIANO_2", "HARPSICHORD", "CLAVINET", "CELESTA", "GLOCKENSPIEL",
      "MUSIC_BOX", "VIBRAPHONE", "MARIMBA", "XYLOPHONE", "TUBULAR_BELLS", "DULCIMER", "DRAWBAR_ORGAN",
      "PERCUSSIVE_ORGAN", "ROCK_ORGAN", "CHURCH_ORGAN", "REED_ORGAN", "ACCORDION", "HARMONICA",
      "TANGO_ACCORDION", "ACOUSTIC_GUITAR_NYLON", "ACOUSTIC_GUITAR_STEEL", "ELECTRIC_GUITAR_JAZZ",
      "ELECTRIC_GUITAR_CLEAN", "ELECTRIC_GUITAR_MUTED", "OVERDRIVEN_GUITAR", "DISTORTION_GUITAR",
      "GUITAR_HARMONICS", "ACOUSTIC_BASS", "ELECTRIC_BASS_FINGER", "ELECTRIC_BASS_PICK", "FRETLESS_BASS",
      "SLAP_BASS_1", "SLAP_BASS_2", "SYNTH_BASS_1", "SYNTH_BASS_2", "VIOLIN", "VIOLA", "CELLO",
      "CONTRABASS", "TREMOLO_STRINGS", "PIZZICATO_STRINGS", "ORCHESTRAL_HARP", "TIMPANI",
      "STRING_ENSEMBLE_1", "STRING_ENSEMBLE_2", "SYNTH_STRINGS_1", "SYNTH_STRINGS_2", "CHOIR_AAHS",
      "VOICE_OOHS", "SYNTH_VOICE", "ORCHESTRA_HIT", "TRUMPET", "TROMBONE", "TUBA", "MUTED_TRUMPET",
      "FRENCH_HORN", "BRASS_SECTION", "SYNTH_BRASS_1", "SYNTH_BRASS_2", "SOPRANO_SAX", "ALTO_SAX",
      "TENOR_SAX", "BARITONE_SAX", "OBOE", "ENGLISH_HORN", "BASSOON", "CLARINET", "PICCOLO", "FLUTE",
      "RECORDER", "PAN_FLUTE", "BLOWN_BOTTLE", "SHAKUHACHI", "WHISTLE", "OCARINA", "LEAD_1_SQUARE",
      "LEAD_2_SAWTOOTH", "LEAD_3_CALLIOPE", "LEAD_4_CHIFF", "LEAD_5_CHARANG", "LEAD_6_VOICE",
      "LEAD_7_FIFTHS", "LEAD_8_BASS_LEAD", "PAD_1_NEW_AGE", "PAD_2_WARM", "PAD_3_POLYSYNTH",
      "PAD_4_CHOIR", "PAD_5_BOWED", "PAD_6_METALLIC", "PAD_7_HALO", "PAD_8_SWEEP", "FX_1_RAIN",
      "FX_2_SOUNDTRACK", "FX_3_CRYSTAL", "FX_4_ATMOSPHERE", "FX_5_BRIGHTNESS", "FX_6_GOBLINS",
      "FX_7_ECHOES", "FX_8_SCI_FI", "SITAR", "BANJO", "SHAMISEN", "KOTO", "KALIMBA", "BAGPIPE",
      "FIDDLE", "SHANAI", "TINKLE_BELL", "AGOGO", "STEEL_DRUMS", "WOODBLOCK", "TAIKO_DRUM",
      "MELODIC_TOM", "SYNTH_DRUM", "REVERSE_CYMBAL", "GUITAR_FRET_NOISE", "BREATH_NOISE", "SEASHORE",
      "BIRD_TWEET", "TELEPHONE_RING", "HELICOPTER", "APPLAUSE", "GUNSHOT"
    ]
  },
  {
    library: "drums",
    detail: "drum / percussion",
    names: [
      "DRUM_CHANNEL", "ACOUSTIC_BASS_DRUM", "BASS_DRUM_1", "SIDE_STICK", "ACOUSTIC_SNARE", "HAND_CLAP",
      "ELECTRIC_SNARE", "LOW_FLOOR_TOM", "CLOSED_HI_HAT", "HIGH_FLOOR_TOM", "PEDAL_HI_HAT", "LOW_TOM",
      "OPEN_HI_HAT", "LOW_MID_TOM", "HI_MID_TOM", "CRASH_CYMBAL_1", "HIGH_TOM", "RIDE_CYMBAL_1",
      "CHINESE_CYMBAL", "RIDE_BELL", "TAMBOURINE", "SPLASH_CYMBAL", "COWBELL", "CRASH_CYMBAL_2",
      "VIBRASLAP", "RIDE_CYMBAL_2", "HI_BONGO", "LOW_BONGO", "MUTE_HI_CONGA", "OPEN_HI_CONGA",
      "LOW_CONGA", "HIGH_TIMBALE", "LOW_TIMBALE", "HIGH_AGOGO", "LOW_AGOGO", "CABASA", "MARACAS",
      "SHORT_WHISTLE", "LONG_WHISTLE", "SHORT_GUIRO", "LONG_GUIRO", "CLAVES", "HI_WOOD_BLOCK",
      "LOW_WOOD_BLOCK", "MUTE_CUICA", "OPEN_CUICA", "MUTE_TRIANGLE", "OPEN_TRIANGLE", "KICK", "SNARE",
      "CLAP", "HIHAT", "OPEN_HAT", "CRASH", "RIDE", "TOM_LOW", "TOM_MID", "TOM_HIGH"
    ]
  },
  {
    library: "scales",
    detail: "scale interval",
    names: [
      "HALF_STEP", "WHOLE_STEP", "MINOR_SECOND", "MAJOR_SECOND", "MINOR_THIRD", "MAJOR_THIRD",
      "PERFECT_FOURTH", "TRITONE", "PERFECT_FIFTH", "MINOR_SIXTH", "MAJOR_SIXTH", "MINOR_SEVENTH",
      "MAJOR_SEVENTH", "OCTAVE"
    ]
  },
  {
    library: "chords",
    detail: "chord interval",
    names: [
      "FLAT_FIFTH", "SHARP_FIFTH", "DIMINISHED_SEVENTH", "DOMINANT_SEVENTH",
      "MINOR_NINTH", "MAJOR_NINTH", "ELEVENTH", "THIRTEENTH"
    ]
  }
];
