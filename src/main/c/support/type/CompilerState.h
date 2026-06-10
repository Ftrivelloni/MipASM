#ifndef COMPILER_STATE_HEADER
#define COMPILER_STATE_HEADER

/**
 * The global state of the compiler. Should transport every data structure
 * needed across the different phases of a compilation.
 */
typedef struct {
	/**
	 * The root node of the AST.
	 */
	void * abstractSyntaxtTree;

	/**
	 * The computed value of the entire program (only for the calculator). You
	 * should change or remove this field, or a random child will die, and it
	 * will be your fault.
	 */
	signed int value;

	/**
	 * Path of the Standard MIDI File the code generator writes. Set from the
	 * command line (the `-o` flag); defaults to "output.mid".
	 */
	const char * midiOutputPath;
	
} CompilerState;

#endif
