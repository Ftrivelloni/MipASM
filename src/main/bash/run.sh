#! /bin/bash

set -euo pipefail

# Runs the development build of the compiler without installing it. All
# arguments are forwarded as-is, and the current working directory is
# preserved so relative paths behave like any other compiler:
#
#   run.sh <input.mip> [-o <output.mid>] [other mipasm options]
exec "$(dirname "$0")/../../../.build/mipasm" "$@"
