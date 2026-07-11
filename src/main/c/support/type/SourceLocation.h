#ifndef SOURCE_LOCATION_HEADER
#define SOURCE_LOCATION_HEADER

/**
 * A span in the source program. Lines and columns are 1-based, matching the
 * convention compilers use in "line:column" diagnostics. A zeroed value means
 * "unknown location" (e.g. a synthesized node).
 */
typedef struct {
	int firstLine;
	int firstColumn;
	int lastLine;
	int lastColumn;
} SourceLocation;

#endif
