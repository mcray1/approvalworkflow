/**
 * speed_controller.h
 * -----------------------------------------------------------------------------
 * Adaptive base-speed logic. Given the current normalized error, its filtered
 * rate of change, and sensor confidence, it decides the BASE forward PWM that
 * the differential steering correction is then added to / subtracted from.
 *
 * Behavior:
 *   - straight sections: ramp the base speed up toward PWM_MAX
 *   - gentle curves    : trim speed only as needed (no harsh braking)
 *   - sharp curves      : cut the base speed to PWM_CURVE, restore gradually
 *   - low confidence    : cap speed (uncertain line => slower)
 *   - optional predictive slow-down from a short error history
 *
 * All transitions are rate-limited (ramped) so the robot never brakes/
 * accelerates abruptly.
 * -----------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "config.h"

class SpeedController {
 public:
  SpeedController();

  void begin();

  // Reset ramp/history state. Call on race start.
  void reset();

  // Compute the base PWM for this cycle.
  //   error       : normalized line error [-~1, +1]
  //   dError      : filtered derivative of error (1/s)
  //   confidence  : sensor confidence (summed calibrated, 0..~8000)
  // Returns a base PWM in [PWM_MIN_MOVING, PWM_MAX].
  uint8_t computeBase(float error, float dError, uint16_t confidence);

  // Introspection.
  uint8_t  base()            const { return base_; }
  float    lastSeverity()    const { return severity_; }   // 0..1
  bool     predicting()      const { return predictActive_; }

 private:
  uint8_t  base_          = PWM_START;  // current ramped base speed
  float    severity_      = 0.0f;       // 0 = straight, 1 = sharpest
  bool     predictActive_ = false;

  // Short error history for prediction (statically sized, no heap).
  float    hist_[PREDICT_HISTORY];
  uint8_t  histIdx_ = 0;
  uint8_t  histLen_ = 0;

  float    predictSeverity_() const;     // 0..1 predicted sharpening
  void     pushHistory_(float error);
  uint8_t  rampToward_(uint8_t current, uint8_t target) const;
};
