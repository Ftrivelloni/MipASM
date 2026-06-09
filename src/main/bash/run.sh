#! /bin/bash

set -euo pipefail

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

# Usage: run.sh <input.mip> [-o <output.mid>] [extra compiler args]
# The input program is passed as a file argument (C-compiler style); any
# remaining arguments (e.g. "-o song.mid") are forwarded as-is.
INPUT="$1"
shift 1
".build/Flex-Bison-Compiler" "$INPUT" "$@"
