#!/bin/sh
# Compile and run the RSSA queue host test; unresolved AqualinkD symbols are never called by it.
set -e
cd "$(dirname "$0")/.."
BIN=$(mktemp "${TMPDIR:-/tmp}/rssa_queue_test.XXXXXX")
trap 'rm -f "$BIN"' EXIT
if [ "$(uname)" = Darwin ]; then UNRESOLVED=-Wl,-undefined,dynamic_lookup; else UNRESOLVED=-Wl,--unresolved-symbols=ignore-all; fi
cc -w -Isource -Isource/mongoose -DAQ_RS16 -DAQ_PDA -DAQ_ONETOUCH -DAQ_IAQTOUCH -DAQ_MANAGER \
   $UNRESOLVED -o "$BIN" test/rssa_queue_test.c -lpthread
"$BIN"
