// Host test for startup_probe_seen (source/startup_probe.h). Build/run: test/startup_probe_test.sh
#include <assert.h>
#include <stdio.h>
#include "startup_probe.h"

int main(void) {
  time_t first = 0;
  // Stock (0): messages never count; only a probe does.
  assert(!startup_probe_seen(false, 100, &first, 0) && first == 100);
  assert(!startup_probe_seen(false, 100000, &first, 0));
  assert(startup_probe_seen(true, 100001, &first, 0));
  // A probe always counts, even before any message.
  first = 0; assert(startup_probe_seen(true, 5, &first, 60) && first == 0);
  // 60 s: messages from t=10; not yet at 69, yes at 70 (the 11-17 iAQ Touch case: resent every ~0.5 s for 30 min).
  first = 0;
  for (time_t t = 10; t < 70; t++) assert(!startup_probe_seen(false, t, &first, 60));
  assert(first == 10);
  assert(startup_probe_seen(false, 70, &first, 60));
  // A normal restart: messages from t=0, probe at t=26 (the longest seen on 11-17): the probe ends it, not the timer.
  first = 0;
  for (time_t t = 1; t < 26; t++) assert(!startup_probe_seen(false, t, &first, 60));
  assert(startup_probe_seen(true, 26, &first, 60));
  // Monotonic clock helper never returns 0 ("no message yet").
  assert(startup_probe_now() > 0);
  printf("startup_probe_test: ok\n");
  return 0;
}
