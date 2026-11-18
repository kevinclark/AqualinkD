#!/bin/sh
# Compile and run the start-up probe host test.
set -e
cd "$(dirname "$0")/.."
BIN=$(mktemp "${TMPDIR:-/tmp}/startup_probe_test.XXXXXX")
trap 'rm -f "$BIN"' EXIT
cc -Wall -Isource -o "$BIN" test/startup_probe_test.c
"$BIN"
