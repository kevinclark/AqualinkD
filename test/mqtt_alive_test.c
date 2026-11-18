// Host test: Alive payload and the HA discovery availability JSON (test/mqtt_alive_test.sh validates the JSON).
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "aqualink.h"
#include "mqtt_alive.h"
#include "aq_mqtt.h"

// The two availability macros, copied by the test script from source/mqtt_discovery.c.
#include "availability_macros.h"

int main(void) {
  assert(strcmp(mqtt_alive_payload(CONNECTED, true), "1") == 0);
  assert(strcmp(mqtt_alive_payload(CONNECTED | ERROR_SERIAL, true), "1") == 0);
  assert(strcmp(mqtt_alive_payload(CONNECTED, false), "2") == 0);   // talking, but no circuit state applied yet
  assert(strcmp(mqtt_alive_payload(CONNECTING, false), "2") == 0);
  assert(strcmp(mqtt_alive_payload(CONNECTING, true), "2") == 0);
  assert(strcmp(mqtt_alive_payload(NOT_CONNECTED, false), "2") == 0);
  assert(strcmp(mqtt_alive_payload(0, false), "2") == 0);
  char buf[512];
  snprintf(buf, sizeof buf, "{" HASS_AVAILABILITY "}", "aqualinkd"); puts(buf);
  snprintf(buf, sizeof buf, "{" HASS_AVAILABILITY_LINK "}", "aqualinkd"); puts(buf);
  return 0;
}
