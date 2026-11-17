/**
 * line_recovery.cpp  -- see line_recovery.h
 */
#include "line_recovery.h"

LineRecovery::LineRecovery() {}

void LineRecovery::begin(int8_t lastDirection, uint32_t nowMs) {
  active_      = true;
  startMs_     = nowMs;
  foundStreak_ = 0;
  // If we had no clear side, bias the search to the right by default; otherwise
  // search toward the side the line was last seen.
  dir_ = (lastDirection < 0) ? -1 : +1;
}

RecoveryStatus LineRecovery::update(bool lineDetected, uint32_t nowMs,
                                    int16_t &outLeft, int16_t &outRight) {
  // Timeout check first (bounded duration guarantee).
  if (elapsed(nowMs) >= RECOVERY_TIMEOUT_MS) {
    outLeft = outRight = 0;
    active_ = false;
    return RecoveryStatus::TIMED_OUT;
  }

  // Re-acquisition: require a short streak of valid detections before resuming.
  if (lineDetected) {
    if (foundStreak_ < 255) foundStreak_++;
    if (foundStreak_ >= LINE_FOUND_DEBOUNCE) {
      outLeft = outRight = 0;   // caller resumes PID immediately
      active_ = false;
      return RecoveryStatus::REACQUIRED;
    }
  } else {
    foundStreak_ = 0;
  }

  // Controlled differential sweep toward the last-known side at reduced speed.
  // Outer wheel forward, inner wheel slowed (or slightly reversed) to pivot
  // without spinning wildly. RECOVERY_TURN_RATIO scales the inner wheel.
  int16_t fast = (int16_t)PWM_RECOVERY;
  int16_t slow = (int16_t)lroundf((float)PWM_RECOVERY *
                                  (1.0f - RECOVERY_TURN_RATIO));
  // Clamp inner wheel to a gentle reverse at most for a tighter arc.
  if (slow < -(int16_t)(PWM_RECOVERY / 2)) slow = -(int16_t)(PWM_RECOVERY / 2);

  if (dir_ > 0) {        // search to the RIGHT: left wheel fast, right slow
    outLeft  = fast;
    outRight = slow;
  } else {               // search to the LEFT: right wheel fast, left slow
    outLeft  = slow;
    outRight = fast;
  }
  return RecoveryStatus::SEARCHING;
}
