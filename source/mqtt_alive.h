#ifndef MQTT_ALIVE_H_
#define MQTT_ALIVE_H_

#include "aqualink.h"

// <mqtt_aq_topic>/Alive payload. "0" = offline (the MQTT last will), "1" = talking to the panel, "2" = running but not yet
// talking to the panel (waiting for its probes at start-up). Until 2026-11 it was "1" from the MQTT connect on, so for
// the first seconds (once, on 2026-11-17, 30 min) of every start Home Assistant showed every circuit at its start-up
// default (pump off, lights off) and automations acted on that. Device entities treat only "1" as available; the status
// line (Display Msg) treats "1" and "2" as available, so "Connecting (waiting for control panel)" stays visible.
#define MQTT_ALIVE_STARTING "2"

static inline const char *mqtt_alive_payload(unsigned int status_mask) {
  return isMASK_SET(status_mask, CONNECTED) ? "1" : MQTT_ALIVE_STARTING;
}

#endif
