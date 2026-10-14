#include "MuteTimer.h"
#include <stdio.h>
#include "MuteButton.h"
#include <stdlib.h>
#include <stdint.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)

int main() {
  MuteButton button;
  CHECK(!button.update(0, true));
  CHECK(!button.update(100, true));
  CHECK(!button.update(200, false));
  CHECK(!button.update(260, false));
  CHECK(!button.update(300, true));
  CHECK(!button.update(320, false)); // Reject short interference.
  CHECK(!button.update(400, true));
  CHECK(!button.update(459, true));
  CHECK(button.update(460, true));
  CHECK(!button.update(40000, true)); // Holding HIGH must not retrigger.
  CHECK(!button.update(40010, false));
  CHECK(!button.update(40030, true)); // Release bounce must not rearm.
  CHECK(!button.update(40100, true));
  CHECK(!button.update(40200, false));
  CHECK(!button.update(40260, false));
  CHECK(!button.update(40300, true));
  CHECK(button.update(40360, true));
  MuteButton wrapButton;
  CHECK(!wrapButton.update(UINT32_MAX - 100, false));
  CHECK(!wrapButton.update(UINT32_MAX - 40, false));
  CHECK(!wrapButton.update(UINT32_MAX - 20, true));
  CHECK(!wrapButton.update(38, true));
  CHECK(wrapButton.update(39, true));
  MuteTimer timer;
  CHECK(!timer.active());
  CHECK(timer.remainingSeconds(1234) == 0);
  timer.update(1000, false);
  CHECK(!timer.active());
  timer.update(1000, true);
  CHECK(timer.active());
  CHECK(timer.remainingSeconds(1000) == 120);
  CHECK(timer.remainingSeconds(1001) == 120);
  CHECK(timer.remainingSeconds(2000) == 119);
  CHECK(timer.remainingSeconds(120999) == 1);
  timer.update(2000, true); // Button bounce must not restart the countdown.
  CHECK(timer.remainingSeconds(2000) == 119);
  timer.update(120999, false);
  CHECK(timer.active());
  CHECK(timer.remainingSeconds(121000) == 0); // HTTP read before next loop.
  timer.update(121000, false);
  CHECK(!timer.active());
  timer.update(122000, true);
  CHECK(timer.active());
  CHECK(timer.remainingSeconds(122000) == 120);
  timer.update(242000, true); // Request during an active interval is consumed.
  CHECK(!timer.active());
  timer.update(242001, true);
  CHECK(timer.remainingSeconds(242001) == 120);

  MuteTimer rollover;
  const uint32_t start = UINT32_MAX - 10000;
  rollover.update(start, true);
  CHECK(rollover.remainingSeconds(uint32_t(start + 10001)) == 110);
  rollover.update(uint32_t(start + 119999), false);
  CHECK(rollover.active());
  CHECK(rollover.remainingSeconds(uint32_t(start + 119999)) == 1);
  rollover.update(uint32_t(start + 120000), false);
  CHECK(!rollover.active());
  CHECK(rollover.remainingSeconds(uint32_t(start + 120000)) == 0);
  puts("PASS: mute button and timer (43 assertions, including millis rollover)");
}
