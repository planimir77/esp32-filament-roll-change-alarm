#pragma once
#include <stdint.h>

// Call under stateMux when shared between the loop and HTTP task.
class MuteTimer {
 public:
  static constexpr uint32_t durationMs = 30000;

  void update(uint32_t now, bool requested) {
    // Preserve the existing rule: presses during a pause do not extend it.
    if (requested && !active_) {
      active_ = true;
      startedAt_ = now;
    }
    if (active_ && uint32_t(now - startedAt_) >= durationMs) active_ = false;
  }

  uint32_t remainingSeconds(uint32_t now) const {
    const uint32_t elapsed = now - startedAt_;
    return active_ && elapsed < durationMs ? (durationMs - elapsed + 999) / 1000 : 0;
  }

  bool active() const { return active_; }

 private:
  bool active_ = false;
  uint32_t startedAt_ = 0;
};
