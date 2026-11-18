#ifndef STARTUP_PROBE_H_
#define STARTUP_PROBE_H_

#include <stdbool.h>
#include <time.h>

// Start-up: AqualinkD waits for the panel to probe each of its IDs before answering anything. A panel that still
// believes an ID is present keeps sending it status polls (or, for an iAQ Touch, its 0x29 start-up and page frames) and
// may not probe it for a long time: on an RS-16 Rev T.1 (2026-11-17) the iAQ Touch ID was not probed in 30 min while the
// panel resent its start-up frame every ~0.5 s, and AqualinkD answered nothing meanwhile. ~50 normal restarts the same
// day all saw every probe within ~26 s. With startup_accept_secs > 0, an ID that has received messages but no probe for
// that long counts as probed. 0 = stock behaviour (wait for the probe).
// Times are monotonic seconds (startup_probe_now): the Pi has no RTC, and NTP can step the wall clock at boot.
// *first is the time of the ID's first non-probe message (0 = none yet). Returns true when this packet completes the
// ID's start-up check.
static inline time_t startup_probe_now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + 1; // never 0, which means "no message yet"
}

static inline bool startup_probe_seen(bool is_probe, time_t now, time_t *first, int accept_secs) {
  if (is_probe)
    return true;
  if (*first == 0)
    *first = now;
  return accept_secs > 0 && now - *first >= accept_secs;
}

#endif
