#!/bin/sh
# Compile utils.c with the MQTT log ring test; unresolved AqualinkD symbols are never called by it.
set -e
cd "$(dirname "$0")/.."
BIN=$(mktemp "${TMPDIR:-/tmp}/mqtt_log_test.XXXXXX")
trap 'rm -f "$BIN"' EXIT
if [ "$(uname)" = Darwin ]; then UNRESOLVED=-Wl,-undefined,dynamic_lookup; else UNRESOLVED=-Wl,--unresolved-symbols=ignore-all; fi
cc -w -Isource -Isource/mongoose -DAQ_RS16 -DAQ_PDA -DAQ_ONETOUCH -DAQ_IAQTOUCH $UNRESOLVED -o "$BIN" test/mqtt_log_test.c source/utils.c -lpthread
"$BIN"
