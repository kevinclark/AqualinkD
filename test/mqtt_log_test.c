// Host test of the MQTT log ring in utils.c: order, newest-64 kept, dropped count, NOTICE+ only, threads.
// Run: test/mqtt_log_test.sh
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include "../source/utils.h"
static void *spam(void *a) { for (int i = 0; i < 20000; i++) LOG(AQUA_LOG, LOG_NOTICE, "thread line %d\n", i); return NULL; }
int main(void) {
  char out[300]; int dropped;
  setLoggingPrms(LOG_NOTICE, false, NULL, NULL);
  mqtt_log_enable(true);
  LOG(AQUA_LOG, LOG_NOTICE, "first %d\n", 1);
  LOG(AQUA_LOG, LOG_INFO, "info is not sent\n");
  LOG(AQUA_LOG, LOG_ERR, "an error\n");
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "first 1") && dropped == 0);
  assert(out[strlen(out)-1] != '\n');
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "an error"));
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  for (int i = 0; i < 100; i++) LOG(AQUA_LOG, LOG_NOTICE, "n %d\n", i);
  assert(mqtt_log_pop(out, sizeof out, &dropped) && strstr(out, "n 36") && dropped == 36);
  int n = 1; while (mqtt_log_pop(out, sizeof out, &dropped)) n++;
  assert(n == 64 && strstr(out, "n 99"));
  setLoggingPrms(LOG_DEBUG, false, NULL, NULL);    // INFO/DEBUG reach _LOG now, but must not be queued
  LOG(AQUA_LOG, LOG_INFO, "info at debug level\n");
  LOG(AQUA_LOG, LOG_DEBUG, "debug line\n");
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  setLoggingPrms(LOG_NOTICE, false, NULL, NULL);
  mqtt_log_enable(false);
  LOG(AQUA_LOG, LOG_ERR, "disabled\n");
  assert(!mqtt_log_pop(out, sizeof out, &dropped));
  mqtt_log_enable(true);
  pthread_t t[3]; for (int i = 0; i < 3; i++) pthread_create(&t[i], NULL, spam, NULL);
  int popped = 0; for (int k = 0; k < 200000; k++) if (mqtt_log_pop(out, sizeof out, &dropped)) { popped++; assert(strstr(out, "thread line")); }
  for (int i = 0; i < 3; i++) pthread_join(t[i], NULL);
  while (mqtt_log_pop(out, sizeof out, &dropped)) popped++;
  printf("threaded: %d popped\nPASS\n", popped);
  return 0;
}
