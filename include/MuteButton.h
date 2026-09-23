#pragma once
#include <stdint.h>

class MuteButton {
 public:
  bool update(uint32_t now, bool high) {
    if (!initialized_ || high != lastHigh_) {
      initialized_ = true;
      lastHigh_ = high;
      changedAt_ = now;
      return false;
    }
    if (uint32_t(now - changedAt_) < 60) return false;
    if (!high) { armed_ = true; return false; }
    if (!armed_) return false;
    armed_ = false;
    return true;
  }
 private:
  bool initialized_ = false;
  bool lastHigh_ = false;
  bool armed_ = false; // Require stable LOW after startup before accepting HIGH.
  uint32_t changedAt_ = 0;
};
