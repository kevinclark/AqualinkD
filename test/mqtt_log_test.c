// Host test of the MQTT log ring in utils.c: order, level filters, network-line rule, repeat collapsing, the
// per-minute cap (repeat notes included), disabled, threads. Uses a fake clock, so it runs instantly.
// Run: test/mqtt_log_test.sh
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include "../source/utils.h"

extern time_t (*_mqtt_log_clock)(time_t *);
static _Atomic time_t fake_now = 6000000;   // a minute boundary
static time_t fake_clock(time_t *t) { return fake_now; }

static char out[300]; static int dropped;
static int drain(void) { int n = 0; while (mqtt_log_pop(out, sizeof out, &dropped)) n++; return n; }
static bool pop_has(const char *s) { return mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, s); }
static void *spam(void *a) { for (int i = 0; i < 20000; i++) LOG(AQUA_LOG, LOG_NOTICE, "thread line %d\n", i); return NULL; }

int main(void) {
  _mqtt_log_clock = fake_clock;
  setLoggingPrms(LOG_NOTICE, false, NULL, NULL);
  mqtt_log_enable(true, LOG_NOTICE);

  // Order, level filter, newline stripped.
  LOG(AQUA_LOG, LOG_NOTICE, "first %d\n", 1);
  LOG(AQUA_LOG, LOG_INFO, "info is not sent\n");
  LOG(AQUA_LOG, LOG_ERR, "an error\n");
  assert(pop_has("first 1") && dropped == 0);
  assert(out[strlen(out)-1] != '\n');
  assert(pop_has("an error"));
  assert(!mqtt_log_pop(out, sizeof out, &dropped));

  // Rate cap: 60 a minute below ERROR (2 used above); network lines only at ERROR; ERROR is never capped.
  for (int i = 0; i < 100; i++) LOG(AQUA_LOG, LOG_NOTICE, "n %d\n", i);
  assert(drain() == 58 && strstr(out, "n 57"));
  LOG(NET_LOG, LOG_NOTICE, "net notice\n");
  LOG(AQUA_LOG, LOG_WARNING, "capped warning\n");
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  LOG(AQUA_LOG, LOG_ERR, "over the cap\n");
  assert(pop_has("over the cap"));

  // Next minute: the summary goes first; INFO/DEBUG reach _LOG at log_level DEBUG but are not queued.
  fake_now += 60;
  setLoggingPrms(LOG_DEBUG, false, NULL, NULL);
  LOG(AQUA_LOG, LOG_INFO, "info at debug level\n");
  LOG(AQUA_LOG, LOG_DEBUG, "debug line\n");
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  LOG(NET_LOG, LOG_INFO, "net info\n"); LOG(NET_LOG, LOG_ERR, "net error 2\n");
  assert(pop_has("not sent to MQTT (over 60 a minute)"));
  assert(pop_has("net error 2"));
  assert(!mqtt_log_pop(out, sizeof out, &dropped));

  // Repeats collapse and are reported with the next different line.
  fake_now += 60;
  for (int i = 0; i < 5; i++) LOG(AQUA_LOG, LOG_WARNING, "same warning\n");
  LOG(AQUA_LOG, LOG_NOTICE, "different\n");
  assert(pop_has("same warning"));
  assert(pop_has("repeated 4 more times"));
  assert(pop_has("different"));
  assert(!mqtt_log_pop(out, sizeof out, &dropped));

  // An identical ERROR within a minute is counted; after a quiet minute the count goes out, and the same ERROR a
  // minute or more after the last one sent goes out again.
  fake_now += 60;
  LOG(AQUA_LOG, LOG_ERR, "Last command failed\n");
  assert(pop_has("Last command failed"));
  fake_now += 10;
  LOG(AQUA_LOG, LOG_ERR, "Last command failed\n");
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  fake_now += 50;
  assert(pop_has("repeated 1 more times"));
  fake_now += 5;
  LOG(AQUA_LOG, LOG_ERR, "Last command failed\n");
  assert(pop_has("Last command failed"));
  assert(!mqtt_log_pop(out, sizeof out, &dropped));

  // Repeat notes count toward the cap: alternating pairs of WARNINGs make one note per pair, and all of it is
  // held to 60 a minute.
  fake_now += 120;
  for (int i = 0; i < 2000; i++) LOG(AQUA_LOG, LOG_WARNING, (i / 2) % 2 ? "warn A\n" : "warn B\n");
  int sent = drain();
  printf("alternating flood: %d sent\n", sent);
  assert(sent <= 60);
  fake_now += 60;
  LOG(AQUA_LOG, LOG_NOTICE, "after the flood\n");
  assert(pop_has("not sent to MQTT (over 60 a minute)"));

  // mqtt_log_level INFO: INFO goes, DEBUG doesn't. ERROR only.
  fake_now += 60; drain();
  mqtt_log_enable(true, LOG_INFO);
  LOG(AQUA_LOG, LOG_INFO, "info at info level\n");
  LOG(AQUA_LOG, LOG_DEBUG, "debug stays\n");
  assert(pop_has("info at info level"));
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  mqtt_log_enable(true, LOG_ERR);
  LOG(AQUA_LOG, LOG_WARNING, "a warning\n");
  LOG(AQUA_LOG, LOG_ERR, "an err\n");
  assert(pop_has("an err"));
  assert(!mqtt_log_pop(out, sizeof out, &dropped));

  // Disabled.
  setLoggingPrms(LOG_NOTICE, false, NULL, NULL);
  mqtt_log_enable(false, LOG_NOTICE);
  LOG(AQUA_LOG, LOG_ERR, "disabled\n");
  assert(!mqtt_log_pop(out, sizeof out, &dropped));

  // Three threads logging while one drains.
  fake_now += 60;
  mqtt_log_enable(true, LOG_NOTICE);
  pthread_t t[3]; for (int i = 0; i < 3; i++) pthread_create(&t[i], NULL, spam, NULL);
  int popped = 0;
  for (int k = 0; k < 200000; k++)
    if (mqtt_log_pop(out, sizeof out, &dropped)) { popped++; assert(strstr(out, "thread line") || strstr(out, "repeated") || strstr(out, "not sent")); }
  for (int i = 0; i < 3; i++) pthread_join(t[i], NULL);
  popped += drain();
  printf("threaded: %d popped\nPASS\n", popped);
  return 0;
}
