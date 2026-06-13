#include "LibraryLocator.h"
#include "Environment.h"
#include "../language/String.h"
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* PRIVATE FUNCTIONS */

/**
 * Returns true when "<candidate>/stdlib" is a directory, which marks the
 * candidate as a MipASM library root.
 */
static bool _isLibraryRoot(const char * candidate) {
	char * marker = concatenate(2, candidate, "/stdlib");
	struct stat status;
	bool isRoot = stat(marker, &status) == 0 && S_ISDIR(status.st_mode);
	free(marker);
	return isRoot;
}

/**
 * Returns the directory holding the running executable (via /proc/self/exe,
 * Linux-only) as a heap string, or NULL when it cannot be determined.
 */
static char * _executableDirectory() {
	char buffer[PATH_MAX];
	ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (length <= 0) {
		return NULL;
	}
	buffer[length] = '\0';
	char * lastSlash = strrchr(buffer, '/');
	if (lastSlash == NULL) {
		return NULL;
	}
	*lastSlash = '\0';
	return concatenate(1, buffer);
}

/* PUBLIC FUNCTIONS */

char * resolveLibraryRoot() {
	const char * fromEnvironment = getStringOrDefault("MIPASM_LIB_PATH", NULL);
	if (fromEnvironment != NULL) {
		return concatenate(1, fromEnvironment);
	}
	char * executableDirectory = _executableDirectory();
	if (executableDirectory == NULL) {
		return NULL;
	}
	const char * relativeCandidates[] = { "/../lib", "/../share/mipasm/lib" };
	for (size_t k = 0; k < sizeof(relativeCandidates) / sizeof(relativeCandidates[0]); ++k) {
		char * candidate = concatenate(2, executableDirectory, relativeCandidates[k]);
		if (_isLibraryRoot(candidate)) {
			free(executableDirectory);
			return candidate;
		}
		free(candidate);
	}
	free(executableDirectory);
	return NULL;
}

char * resolveLibraryFile(const char * libraryRoot, const char * libraryName) {
	if (libraryRoot == NULL || libraryName == NULL) {
		return NULL;
	}
	char * path = concatenate(4, libraryRoot, "/", libraryName, ".mip");
	char * canonicalPath = realpath(path, NULL);
	free(path);
	return canonicalPath;
}
