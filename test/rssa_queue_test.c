// Host test of the RS Serial Adapter queues (serialadapter.c): commands go out in order ahead of periodic polls,
// polls go one at a time and never as a setpoint value, a setpoint pair stays back to back, overflow never splits it.
// Run: test/rssa_queue_test.sh
#include "../source/serialadapter.c"
#include <assert.h>

struct aqconfig _aqconfig_;
void LOG(const logmask_t from, const int msg_level, const char *format, ...) {}

static void expect(unsigned char src, unsigned char a, unsigned char b, const char *what) {
  unsigned char *c = get_rssa_cmd(src);
  remove_rssa_cmd();
  if (c[2] != a || c[3] != b) { printf("FAIL %s: got %02x %02x want %02x %02x\n", what, c[2], c[3], a, b); exit(1); }
}
static _Atomic int stop = 0;
static void *pair_pusher(void *a) { while (!stop) queue_aqualink_rssadapter_setpoint(RS_SA_POOLSP, 77); return NULL; }
static void *set_pusher(void *a) { while (!stop) { rssadapter_device_state(0x2c, RS_SA_ON); rssadapter_device_poll(0x2d); } return NULL; }
static void reset(void) { _rssa_q_length = 0; _rssa_poll_length = 0; _rssa_last_was_vbutton_set = false; _rssa_last_was_readysp = false; }

int main(void) {
  // 1. A light set queued after a whole poll cycle goes out first; the polls follow one per status message.
  reset();
  for (int i = 0; i < 6; i++) rssadapter_device_poll(0x2c + i);
  push_rssa_poll(getPoolSP); push_rssa_poll(getSpaSP);
  push_rssa_poll(getAux12); push_rssa_poll(getAux13); push_rssa_poll(getAux14); push_rssa_poll(getAux15);
  assert(_rssa_poll_length == 12);
  rssadapter_device_state(0x30, RS_SA_ON); rssadapter_device_state(0x30, 0x00);
  expect(CMD_STATUS, RS_SA_ON, 0x30, "set before polls");
  expect(CMD_STATUS, 0x00, 0x30, "its read-back");
  expect(CMD_STATUS, 0x00, 0x2c, "then polls in order");

  // 1b. A room's virtual button sets go out back to back; their read-backs follow as polls.
  reset();
  aqkey b[3] = {{.label="A", .rssa_id=0x2c}, {.label="B", .rssa_id=0x2d}, {.label="C", .rssa_id=0x2e}};
  rssadapter_device_poll(0x2d);                      // a periodic poll already waiting for B
  for (int i = 0; i < 3; i++) set_aqualink_rssadapter_vbutton_state(&b[i], true);
  expect(CMD_STATUS, RS_SA_ON, 0x2c, "set A");
  expect(CMD_STATUS, RS_SA_ON, 0x2d, "set B");
  expect(CMD_STATUS, RS_SA_ON, 0x2e, "set C");
  expect(CMD_STATUS, 0x00, 0x2d, "read-back B (deduped with the waiting poll)");
  expect(CMD_STATUS, 0x00, 0x2c, "read-back A");
  expect(CMD_STATUS, 0x00, 0x2e, "read-back C");
  expect(CMD_STATUS, 0x00, 0x00, "then idle");

  // 2. Duplicate polls collapse.
  reset();
  for (int k = 0; k < 3; k++) rssadapter_device_poll(0x31);
  assert(_rssa_poll_length == 1);

  // 3. Setpoint pair with polls waiting and a light command arriving between its halves going out:
  //    the value follows readySP; a 0x07 never takes a poll; the light set comes after.
  reset();
  rssadapter_device_poll(0x2c);
  queue_aqualink_rssadapter_setpoint(RS_SA_POOLSP, 0x2c);       // value equal to a vbutton ID on purpose
  expect(CMD_STATUS, RS_SA_POOLSP, 0x35, "readySP");
  rssadapter_device_state(0x2d, RS_SA_OFF);                      // arrives between the halves
  expect(0x07, 0x00, 0x2c, "value next");
  expect(CMD_STATUS, RS_SA_OFF, 0x2d, "set after the pair");
  expect(CMD_STATUS, 0x00, 0x2c, "poll last");

  // 4. A 0x07 with only polls waiting gets a null, not a poll.
  reset();
  rssadapter_device_poll(0x2c);
  expect(0x07, 0x00, 0x00, "no poll on 0x07");
  assert(_rssa_poll_length == 1);

  // 5. A 0x07 right after a virtual button set gets a null (from d0aa0b7) and consumes nothing.
  reset();
  rssadapter_device_state(0x2d, RS_SA_ON); rssadapter_device_state(0x2d, 0x00);
  expect(CMD_STATUS, RS_SA_ON, 0x2d, "set");
  expect(0x07, 0x00, 0x00, "null to 0x07 after set");
  expect(CMD_STATUS, 0x00, 0x2d, "read-back still queued");

  // 5b. A late 0x07 (a status message came between readySP and it) gets a null; queued commands stay.
  reset();
  queue_aqualink_rssadapter_setpoint(RS_SA_POOLSP, 80);
  rssadapter_device_state(0x2c, RS_SA_OFF);
  expect(CMD_STATUS, RS_SA_POOLSP, 0x35, "readySP");
  expect(CMD_STATUS, 0x00, 80, "value went out on a status");
  expect(0x07, 0x00, 0x00, "late 0x07 gets null");
  expect(CMD_STATUS, RS_SA_OFF, 0x2c, "light set still queued");

  // 6. Overflow: a pair that doesn't fit is refused whole, never half-queued.
  reset();
  for (int i = 0; i < RSSA_QLEN - 1; i++) rssadapter_device_state(0x24, 0x00);
  queue_aqualink_rssadapter_setpoint(RS_SA_SPASP, 100);
  assert(_rssa_q_length == RSSA_QLEN - 1);
  rssadapter_device_state(0x24, 0x00);
  assert(_rssa_q_length == RSSA_QLEN);

  // 7. Two threads pushing pairs and sets while the main thread drains: every readySP is followed by its value.
  reset();
  pthread_t t1, t2;
  pthread_create(&t1, NULL, pair_pusher, NULL);
  pthread_create(&t2, NULL, set_pusher, NULL);
  int pairs = 0;
  bool want_value = false;
  for (long n = 0; n < 4000000 && pairs < 20000; n++) {
    unsigned char *c = get_rssa_cmd(want_value ? 0x07 : CMD_STATUS);
    unsigned char b2 = c[2], b3 = c[3];
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
