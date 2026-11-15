// Host test of the MQTT log ring in utils.c: order, newest-64 kept, dropped count, NOTICE+ only, threads.
// Run: test/mqtt_log_test.sh
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include "../source/utils.h"
#include <time.h>
#include <unistd.h>
static void sleep_to_next_minute(void) { time_t t = time(NULL); sleep(60 - (int)(t % 60) + 1); }
static void *spam(void *a) { for (int i = 0; i < 20000; i++) LOG(AQUA_LOG, LOG_NOTICE, "thread line %d\n", i); return NULL; }
int main(void) {
  char out[300]; int dropped;
  if (time(NULL) % 60 > 50) sleep_to_next_minute();   // keep the rate-cap checks inside one minute
  setLoggingPrms(LOG_NOTICE, false, NULL, NULL);
  mqtt_log_enable(true, LOG_NOTICE);
  LOG(AQUA_LOG, LOG_NOTICE, "first %d\n", 1);
  LOG(AQUA_LOG, LOG_INFO, "info is not sent\n");
  LOG(AQUA_LOG, LOG_ERR, "an error\n");
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "first 1") && dropped == 0);
  assert(out[strlen(out)-1] != '\n');
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "an error"));
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  for (int i = 0; i < 100; i++) LOG(AQUA_LOG, LOG_NOTICE, "n %d\n", i);   // rate cap: 60 a minute (3 used above)
  int n = 0; while (mqtt_log_pop(out, sizeof out, &dropped)) n++;
  assert(n == 58 && strstr(out, "n 57"));               // 2 lines pushed above + 58 = 60
  LOG(NET_LOG, LOG_NOTICE, "net notice\n");                         // network lines only at ERROR
  LOG(AQUA_LOG, LOG_WARNING, "capped warning\n");                    // cap reached: below ERROR waits
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  LOG(AQUA_LOG, LOG_ERR, "over the cap\n");                          // ERROR always goes
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "over the cap"));
  printf("waiting for the next minute (rate-cap window)...\n");
  sleep_to_next_minute();
  setLoggingPrms(LOG_DEBUG, false, NULL, NULL);    // INFO/DEBUG reach _LOG now, but must not be queued
  LOG(AQUA_LOG, LOG_INFO, "info at debug level\n");
  LOG(AQUA_LOG, LOG_DEBUG, "debug line\n");
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  LOG(NET_LOG, LOG_INFO, "net info\n"); LOG(NET_LOG, LOG_ERR, "net error 2\n");   // first push of the new minute
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "not sent to MQTT (over 60 a minute)"));
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "net error 2"));
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  for (int i = 0; i < 5; i++) LOG(AQUA_LOG, LOG_WARNING, "same warning\n");   // repeats collapse
  LOG(AQUA_LOG, LOG_NOTICE, "different\n");
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "same warning"));
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "repeated 4 more times"));
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "different"));
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  mqtt_log_enable(true, LOG_INFO);                  // mqtt_log_level INFO: INFO goes, DEBUG doesn't
  LOG(AQUA_LOG, LOG_INFO, "info at info level\n");
  LOG(AQUA_LOG, LOG_DEBUG, "debug stays\n");
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "info at info level"));
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  mqtt_log_enable(true, LOG_ERR);                   // ERROR only
  LOG(AQUA_LOG, LOG_WARNING, "a warning\n");
  LOG(AQUA_LOG, LOG_ERR, "an err\n");
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "an err"));
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  setLoggingPrms(LOG_NOTICE, false, NULL, NULL);
  mqtt_log_enable(false, LOG_NOTICE);
  LOG(AQUA_LOG, LOG_ERR, "disabled\n");
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  mqtt_log_enable(true, LOG_NOTICE);
  pthread_t t[3]; for (int i = 0; i < 3; i++) pthread_create(&t[i], NULL, spam, NULL);
  int popped = 0; for (int k = 0; k < 200000; k++) if (mqtt_log_pop(out, sizeof out, &dropped)) { popped++; assert(strstr(out, "thread line") || strstr(out, "repeated") || strstr(out, "not sent")); }
  for (int i = 0; i < 3; i++) pthread_join(t[i], NULL);
  while (mqtt_log_pop(out, sizeof out, &dropped)) popped++;
  printf("threaded: %d popped\nPASS\n", popped);
  return 0;
}
