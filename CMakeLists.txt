# Minimum required version of CMake. Can be modified at your own risk.
# @see https://cmake.org/download/
cmake_minimum_required(VERSION 3.25)

# Name and version of the project, and language to use (C, in this case).
project(MipASM VERSION 2.0.0 LANGUAGES C)

# AddressSanitizer is for development builds; switch it off for installable
# release binaries: cmake -DMIPASM_SANITIZE=OFF ...
option(MIPASM_SANITIZE "Build with AddressSanitizer (development builds)" ON)

# Selects the best strategy according to the available compiler in the system.
# @see https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_ID.html
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
	message(NOTICE "The C compiler is GCC.")

	# Compiles the parser with Bison, generating counter-examples in case S/R or R/R conflicts exists.
	add_custom_command(
		OUTPUT ${CMAKE_SOURCE_DIR}/src/main/c/frontend/syntactic-analysis/BisonParser.c ${CMAKE_SOURCE_DIR}/src/main/c/frontend/syntactic-analysis/BisonParser.h
		COMMAND ${CMAKE_SOURCE_DIR}/src/main/bash/bison.sh
	)

	# Options for GCC.
	if (MIPASM_SANITIZE)
		add_compile_options(-fsanitize=address)
		add_link_options(-fsanitize=address)
	endif ()
	add_compile_options(-O3)
	add_compile_options(-static-libgcc)
	add_compile_options(-std=gnu99)

	# Compiles the scanner with Flex.
	add_custom_command(
		OUTPUT ${CMAKE_SOURCE_DIR}/src/main/c/frontend/lexical-analysis/FlexScanner.c ${CMAKE_SOURCE_DIR}/src/main/c/frontend/lexical-analysis/FlexScanner.h
		COMMAND ${CMAKE_SOURCE_DIR}/src/main/bash/flex.sh
		DEPENDS ${CMAKE_SOURCE_DIR}/src/main/c/frontend/syntactic-analysis/BisonParser.c ${CMAKE_SOURCE_DIR}/src/main/c/frontend/syntactic-analysis/BisonParser.h
	)

	# Defines the entry-point of the application, and the source-codes (*.c extension).
	# The header files (*.h extension), are automatically included from the source-codes.
	add_executable(mipasm
		src/main/c/backend/code-generation/Generator.c
		src/main/c/backend/code-generation/MidiEmitter.c
		src/main/c/backend/semantic-analysis/SemanticAnalyzer.c
		src/main/c/EntryPoint.c
		src/main/c/frontend/Frontend.c
		src/main/c/frontend/lexical-analysis/FlexActions.c
		src/main/c/frontend/lexical-analysis/FlexScanner.c
		src/main/c/frontend/syntactic-analysis/AbstractSyntaxTree.c
		src/main/c/frontend/syntactic-analysis/BisonActions.c
		src/main/c/frontend/syntactic-analysis/BisonParser.c
		src/main/c/support/configuration/Environment.c
		src/main/c/support/configuration/LibraryLocator.c
		src/main/c/support/language/String.c
		src/main/c/support/logging/Logger.c
		# Add more *.c files if needed (otherwise, they won't be compiled).
		# ...
	)

	# The version reported by "mipasm --version".
	target_compile_definitions(mipasm PRIVATE MIPASM_VERSION="${PROJECT_VERSION}")

	# Installs the compiler into the standard binary directory, so that
	# "cmake --install <builddir>" makes the "mipasm" command available,
	# and the standard library where the compiler searches for it
	# (<bindir>/../share/mipasm/lib).
	include(GNUInstallDirs)
	install(TARGETS mipasm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/ DESTINATION ${CMAKE_INSTALL_DATADIR}/mipasm/lib)
else ()
	message(NOTICE "The C compiler is unknown.")
endif ()
