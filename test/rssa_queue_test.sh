#!/bin/sh
# Compile and run the RSSA queue host test; unresolved AqualinkD symbols are never called by it.
set -e
cd "$(dirname "$0")/.."
cc -w -Isource -Isource/mongoose -DAQ_RS16 -DAQ_PDA -DAQ_ONETOUCH -DAQ_IAQTOUCH -DAQ_MANAGER \
   -Wl,-undefined,dynamic_lookup -o /tmp/rssa_queue_test test/rssa_queue_test.c 2>/dev/null || \
cc -w -Isource -Isource/mongoose -DAQ_RS16 -DAQ_PDA -DAQ_ONETOUCH -DAQ_IAQTOUCH -DAQ_MANAGER \
   -Wl,--unresolved-symbols=ignore-all -o /tmp/rssa_queue_test test/rssa_queue_test.c
/tmp/rssa_queue_test
