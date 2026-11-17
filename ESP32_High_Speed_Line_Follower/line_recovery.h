/**
 * line_recovery.h
 * -----------------------------------------------------------------------------
 * Controlled search maneuver used when the line is lost. Given the last-known
 * line direction, it produces differential motor commands to sweep back toward
 * the line, confirms re-acquisition across several samples, and enforces a hard
 * timeout that results in a safe stop.
 *
 * The recovery module does NOT touch the hardware directly; it returns the
 * desired left/right speeds and a status so the race manager stays in control
 * of the state machine and motor output.
 * -----------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "config.h"

enum class RecoveryStatus : uint8_t {
  SEARCHING,    // still sweeping for the line
  REACQUIRED,   // line confirmed; resume PID tracking
  TIMED_OUT     // exceeded RECOVERY_TIMEOUT_MS; caller must safe-stop
};

class LineRecovery {
 public:
  LineRecovery();

  // Begin a recovery episode. lastDirection: -1 line was left, +1 right,
  // 0 unknown (defaults to searching toward the last non-zero side).
  void begin(int8_t lastDirection, uint32_t nowMs);

  // Advance one control cycle.
  //   lineDetected : current debounced detection from the sensor module
  //   nowMs        : millis() timestamp
  // Outputs desired signed wheel speeds in outLeft/outRight (0..255 domain).
  RecoveryStatus update(bool lineDetected, uint32_t nowMs,
                        int16_t &outLeft, int16_t &outRight);

  uint32_t elapsed(uint32_t nowMs) const { return nowMs - startMs_; }
  bool     active() const { return active_; }

 private:
  bool     active_      = false;
  int8_t   dir_         = 1;     // search direction (+1 right, -1 left)
  uint32_t startMs_     = 0;
  uint8_t  foundStreak_ = 0;
};
