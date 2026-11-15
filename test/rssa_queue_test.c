// Host test of the RS Serial Adapter queue rules (serialadapter.c): vbutton sets jump queued polls, never split a
// setpoint pair, and the latest on/off per ID wins. Build: see test/rssa_queue_test.sh
#include "../source/serialadapter.c"
#include <assert.h>

struct aqconfig _aqconfig_;
void LOG(const logmask_t from, const int msg_level, const char *format, ...) {}

static void expect(unsigned char src, unsigned char a, unsigned char b, const char *what) {
  unsigned char *c = get_rssa_cmd(src);
  remove_rssa_cmd();
  if (c[2] != a || c[3] != b) { printf("FAIL %s: got %02x %02x want %02x %02x\n", what, c[2], c[3], a, b); exit(1); }
}
static void reset(void) { _rssa_q_length = 0; _rssa_awaiting_value = false; _rssa_last_was_vbutton_set = false; }

int main(void) {
  // 1. A set and its read-back go ahead of a poll cycle's queries.
  reset();
  for (int i = 0; i < 12; i++) rssadapter_device_state(0x24 + i, 0x00);
  push_rssa_vbutton_set_front(0x30, RS_SA_ON);
  expect(CMD_STATUS, RS_SA_ON, 0x30, "set first");
  expect(CMD_STATUS, 0x00, 0x30, "read-back second");
  expect(CMD_STATUS, 0x00, 0x24, "polls keep their order");
  assert(_rssa_q_length == 11);

  // 2. After the first half of a setpoint went out, the value stays at the head.
  reset();
  queue_aqualink_rssadapter_setpoint(0x0a, 85);   // readySP {..,0x0a,0x35}, setSP {..,0x00,85}
  rssadapter_device_state(0x24, 0x00);
  expect(CMD_STATUS, 0x0a, 0x35, "readySP out");
  push_rssa_vbutton_set_front(0x31, RS_SA_OFF);
  expect(0x07, 0x00, 85, "value follows readySP");
  expect(CMD_STATUS, RS_SA_OFF, 0x31, "set next");
  expect(CMD_STATUS, 0x00, 0x31, "read-back");
  expect(CMD_STATUS, 0x00, 0x24, "poll");

  // 2b. A queued, not yet sent setpoint pair stays together behind the set.
  reset();
  queue_aqualink_rssadapter_setpoint(0x0a, 85);
  push_rssa_vbutton_set_front(0x31, RS_SA_ON);
  expect(CMD_STATUS, RS_SA_ON, 0x31, "set before pair");
  expect(CMD_STATUS, 0x00, 0x31, "read-back");
  expect(CMD_STATUS, 0x0a, 0x35, "readySP");
  expect(0x07, 0x00, 85, "value");

  // 3. ON then OFF before either went out: only OFF goes.
  reset();
  for (int i = 0; i < 3; i++) rssadapter_device_state(0x24 + i, 0x00);
  push_rssa_vbutton_set_front(0x30, RS_SA_ON);
  push_rssa_vbutton_set_front(0x2c, RS_SA_ON);
  push_rssa_vbutton_set_front(0x30, RS_SA_OFF);
  expect(CMD_STATUS, RS_SA_OFF, 0x30, "latest wins");
  expect(CMD_STATUS, 0x00, 0x30, "read-back");
  expect(CMD_STATUS, RS_SA_ON, 0x2c, "other ID kept");
  expect(CMD_STATUS, 0x00, 0x2c, "its read-back");
  expect(CMD_STATUS, 0x00, 0x30, "old read-back harmless");
  for (int i = 0; i < 3; i++) expect(CMD_STATUS, 0x00, 0x24 + i, "polls");
  expect(CMD_STATUS, 0x00, 0x00, "empty queue sends null");

  // 4. A 0x07 right after a vbutton set gets a null, and nothing is consumed.
  reset();
  push_rssa_vbutton_set_front(0x2d, RS_SA_ON);
  expect(CMD_STATUS, RS_SA_ON, 0x2d, "set");
  expect(0x07, 0x00, 0x00, "null to 0x07 after set");
  expect(CMD_STATUS, 0x00, 0x2d, "read-back still queued");

  // 5. Overflow refuses, never corrupts.
  reset();
  for (int i = 0; i < RSSA_QLEN - 1; i++) rssadapter_device_state(0x24, 0x00);
  assert(!push_rssa_vbutton_set_front(0x30, RS_SA_ON));
  assert(_rssa_q_length == RSSA_QLEN - 1);

  printf("PASS\n");
  return 0;
}
