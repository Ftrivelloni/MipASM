#! /bin/bash

set -u

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

GREEN='\033[0;32m'
RED='\033[0;31m'
OFF='\033[0m'
STATUS=0
BINARY=".build/mipasm"

echo "Compiler should accept..."
echo ""

for test in $(ls test/c/accept/); do
	cat "test/c/accept/$test" | "$BINARY" - -o /dev/null >/dev/null 2>&1
	RESULT="$?"
	if [ "$RESULT" == "0" ]; then
		echo -e "    $test, ${GREEN}and it does${OFF} (status $RESULT)"
	else
		STATUS=1
		echo -e "    $test, ${RED}but it rejects${OFF} (status $RESULT)"
	fi
done
echo ""

echo "Compiler should reject..."
echo ""

for test in $(ls test/c/reject/); do
	cat "test/c/reject/$test" | "$BINARY" - -o /dev/null >/dev/null 2>&1
	RESULT="$?"
	if [ "$RESULT" != "0" ]; then
		echo -e "    $test, ${GREEN}and it does${OFF} (status $RESULT)"
	else
		STATUS=1
		echo -e "    $test, ${RED}but it accepts${OFF} (status $RESULT)"
	fi
done
echo ""

echo "Command-line interface..."
echo ""

# Each check runs a CLI scenario and compares the outcome (exit status, plus
# an output file or message where relevant) against the expected behaviour.
check() {
	local name="$1"
	local expected="$2"	# "pass" or "fail"
	local result="$3"
	if { [ "$expected" == "pass" ] && [ "$result" == "0" ]; } || { [ "$expected" == "fail" ] && [ "$result" != "0" ]; }; then
		echo -e "    $name, ${GREEN}and it does${OFF} (status $result)"
	else
		STATUS=1
		echo -e "    $name, ${RED}but it does not${OFF} (status $result)"
	fi
}

BINARY_ABS="$(pwd)/$BINARY"
SAMPLE="test/c/accept/09-mipasm-basic"
TMP="$(mktemp -d)"

"$BINARY" --help 2>/dev/null | grep -q "Usage:"
check "--help should print usage and succeed" "pass" "$?"

"$BINARY" -h >/dev/null 2>&1
check "-h should succeed" "pass" "$?"

"$BINARY" --version 2>/dev/null | grep -q "[0-9]"
check "--version should print a version and succeed" "pass" "$?"

"$BINARY" --bogus-flag >/dev/null 2>&1
check "an unknown option should be rejected" "fail" "$?"

"$BINARY" -o >/dev/null 2>&1
check "-o without a filename should be rejected" "fail" "$?"

"$BINARY" a.mip b.mip >/dev/null 2>&1
check "multiple input files should be rejected" "fail" "$?"

cp "$SAMPLE" "$TMP/song.mip"
(cd / && "$BINARY_ABS" "$TMP/song.mip" >/dev/null 2>&1) && [ -f "$TMP/song.mid" ]
check "song.mip should compile to song.mid next to the input" "pass" "$?"

cp "$SAMPLE" "$TMP/plain"
"$BINARY" "$TMP/plain" >/dev/null 2>&1 && [ -f "$TMP/plain.mid" ]
check "an input without a .mip suffix should compile to <input>.mid" "pass" "$?"

(cd "$TMP" && cat "$OLDPWD/$SAMPLE" | "$BINARY_ABS" - >/dev/null 2>&1) && [ -f "$TMP/a.mid" ]
check "standard input ('-') should compile to a.mid" "pass" "$?"

rm -rf "$TMP"
echo ""

echo "All done."
exit $STATUS
