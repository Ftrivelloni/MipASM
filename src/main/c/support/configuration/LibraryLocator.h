#ifndef LIBRARY_LOCATOR_HEADER
#define LIBRARY_LOCATOR_HEADER

/**
 * Resolves the root directory of the MipASM standard library. The search
 * order is: the MIPASM_LIB_PATH environment variable (returned verbatim),
 * then "<executable dir>/../lib" (development layout, e.g. ".build/mipasm"
 * next to the repository's "lib/"), then "<executable dir>/../share/mipasm/
 * lib" (installed layout). The exe-relative candidates are only accepted
 * when they contain a "stdlib" subdirectory, so unrelated "lib" directories
 * (e.g. "/usr/local/lib") are never mistaken for the library root. Returns
 * a heap string that must be freed, or NULL when no root can be found.
 */
char * resolveLibraryRoot();

/**
 * Resolves a library name (e.g. "stdlib/audio") to the canonical absolute
 * path of "<libraryRoot>/<libraryName>.mip". Returns a heap string that must
 * be freed, or NULL when the file does not exist. Being canonical, the
 * result can double as an include-once deduplication key.
 */
char * resolveLibraryFile(const char * libraryRoot, const char * libraryName);

#endif
