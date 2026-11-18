#!/bin/sh
# Compile and run the Alive/availability host test; checks the availability JSON parses and the templates map 0/1/2.
set -e
cd "$(dirname "$0")/.."
TMP=$(mktemp -d "${TMPDIR:-/tmp}/mqtt_alive_test.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
awk '/^#define HASS_AVAILABILITY /,/MQTT_LWM_TOPIC "\\""$/' source/mqtt_discovery.c > "$TMP/availability_macros.h"
awk '/^#define HASS_AVAILABILITY_LINK /,/MQTT_LWM_TOPIC "\\""$/' source/mqtt_discovery.c >> "$TMP/availability_macros.h"
cc -w -I"$TMP" -Isource -Isource/mongoose -o "$TMP/t" test/mqtt_alive_test.c
"$TMP/t" > "$TMP/out"
python3 - "$TMP/out" <<'PY'
import json, sys
dev, link = [json.loads(l) for l in open(sys.argv[1])]
assert dev["topic"] == link["topic"] == "aqualinkd/Alive"
def render(t, v):  # the two templates are simple enough to evaluate without Jinja
    return {"{{ '1' if value == '1' else '0' }}": '1' if v == '1' else '0',
            "{{ '0' if value == '0' else '1' }}": '0' if v == '0' else '1'}[t]
assert [render(dev["value_template"], v) for v in "012"] == ["0", "1", "0"]
assert [render(link["value_template"], v) for v in "012"] == ["0", "1", "1"]
print("mqtt_alive_test: ok")
PY
