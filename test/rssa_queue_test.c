// Host test of the RS Serial Adapter queue (serialadapter.c): commands go out in order, a setpoint pair stays back
// to back (also with other threads pushing), overflow never splits it.
// Run: test/rssa_queue_test.sh
#include "../source/serialadapter.c"
#include <assert.h>

struct aqconfig _aqconfig_;
void LOG(const logmask_t from, const int msg_level, const char *format, ...) {}

static void expect(unsigned char src, unsigned char a, unsigned char b, const char *what) {
  unsigned char *p = get_rssa_cmd(src), c[4];
  memcpy(c, p, 4);              // as aqualinkd.c does: send it, then remove_rssa_cmd()
  remove_rssa_cmd();
  if (c[2] != a || c[3] != b) { printf("FAIL %s: got %02x %02x want %02x %02x\n", what, c[2], c[3], a, b); exit(1); }
}
static _Atomic int stop = 0;
static void *pair_pusher(void *a) { while (!stop) queue_aqualink_rssadapter_setpoint(RS_SA_POOLSP, 77); return NULL; }
static void *set_pusher(void *a) { while (!stop) { rssadapter_device_state(0x16, RS_SA_ON); rssadapter_device_state(0x17, 0x00); } return NULL; }
static void reset(void) { _rssa_q_length = 0; }

int main(void) {
  // 1. Commands go out in the order queued, one per status message, and the next comes only after the last is gone.
  reset();
  rssadapter_device_state(0x16, RS_SA_ON); rssadapter_device_state(0x16, 0x00);
  expect(CMD_STATUS, RS_SA_ON, 0x16, "first");
  expect(CMD_STATUS, 0x00, 0x16, "second");
  expect(CMD_STATUS, 0x00, 0x00, "then null");

  // 3. Setpoint pair with a command arriving between its halves going out: the value follows readySP.
  reset();
  queue_aqualink_rssadapter_setpoint(RS_SA_POOLSP, 0x16);       // value equal to a device ID on purpose
  expect(CMD_STATUS, RS_SA_POOLSP, 0x35, "readySP");
  rssadapter_device_state(0x17, RS_SA_OFF);                      // arrives between the halves
  expect(0x07, 0x00, 0x16, "value next");
  expect(CMD_STATUS, RS_SA_OFF, 0x17, "command after the pair");

  // 6. Overflow: a pair that doesn't fit is refused whole, never half-queued.
  reset();
  for (int i = 0; i < RSSA_QLEN - 1; i++) rssadapter_device_state(0x16, 0x00);
  queue_aqualink_rssadapter_setpoint(RS_SA_SPASP, 100);
  assert(_rssa_q_length == RSSA_QLEN - 1);
  rssadapter_device_state(0x16, 0x00);
  assert(_rssa_q_length == RSSA_QLEN);

  // 7. Two threads pushing pairs and on/off commands while the main thread drains: every readySP is followed by its
  //    value. (Stock code fails this: the two halves are separate unlocked pushes.)
  reset();
  pthread_t t1, t2;
  pthread_create(&t1, NULL, pair_pusher, NULL);
  pthread_create(&t2, NULL, set_pusher, NULL);
  int pairs = 0;
  bool want_value = false;
  for (long n = 0; n < 4000000 && pairs < 20000; n++) {
    unsigned char *c = get_rssa_cmd(want_value ? 0x07 : CMD_STATUS);
    unsigned char b2 = c[2], b3 = c[3];
    remove_rssa_cmd();
    if (want_value) {
      if (b2 != 0x00 || b3 != 77) { printf("FAIL threaded: after readySP got %02x %02x\n", b2, b3); exit(1); }
      want_value = false; pairs++;
    } else if (b3 == 0x35 && b2 != 0x00) {
      want_value = true;
    }
  }
  stop = 1; pthread_join(t1, NULL); pthread_join(t2, NULL);
  printf("threaded: %d pairs intact\n", pairs);
  assert(pairs > 1000);
  printf("PASS\n");
  return 0;
}
