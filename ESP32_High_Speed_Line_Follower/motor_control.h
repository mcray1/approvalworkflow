/**
 * motor_control.h
 * -----------------------------------------------------------------------------
 * TB6612FNG dual-motor interface for the ESP32 using the Arduino-ESP32 3.x
 * LEDC API (ledcAttach / ledcWrite).
 *
 * Responsibilities:
 *   - PWM generation (hardware LEDC), independent per motor
 *   - direction control (forward / reverse / brake / coast)
 *   - configurable polarity inversion and left/right trim
 *   - acceleration/deceleration slew limiting
 *   - command saturation to the safe PWM range
 *   - emergency stop + driver standby handling
 *
 * Signed speeds are used throughout: -MOTOR_PWM_MAX .. +MOTOR_PWM_MAX, where
 * positive = forward. The caller works in the 0..255 domain (8-bit).
 * -----------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "config.h"

class MotorController {
 public:
  MotorController();

  // Attach LEDC channels, configure direction pins and standby. Leaves the
  // driver DISABLED (motors inhibited) until enable() is called.
  void begin();

  // Enable/disable the driver output stage (TB6612 STBY). When disabled, motors
  // coast and setSpeeds() has no physical effect until re-enabled.
  void enable();
  void disable();
  bool isEnabled() const { return enabled_; }

  // Command target signed speeds (-255..+255). Values are clamped, trimmed and
  // reached through the slew limiter on each apply(). This only sets targets;
  // call apply() once per control cycle to move the outputs toward them.
  void setSpeeds(int16_t left, int16_t right);

  // Advance the actual outputs toward the targets by at most the configured
  // accel/decel step, then write PWM + direction to the hardware. Call every
  // control cycle.
  void apply();

  // Immediately zero targets AND outputs and actively brake, then (optionally)
  // the caller may disable(). Use for emergency / safe stop.
  void emergencyStop();

  // Introspection (telemetry).
  int16_t targetLeft()  const { return tgtL_; }
  int16_t targetRight() const { return tgtR_; }
  int16_t outputLeft()  const { return outL_; }
  int16_t outputRight() const { return outR_; }

 private:
  bool    enabled_ = false;
  int16_t tgtL_ = 0, tgtR_ = 0;   // commanded targets
  int16_t outL_ = 0, outR_ = 0;   // current (slewed) outputs

  static int16_t clampSpeed_(int32_t v);
  static int16_t slew_(int16_t current, int16_t target);
  void writeMotor_(uint8_t in1, uint8_t in2, uint8_t pwmPin,
                   int16_t signedSpeed, bool invert, float trim);
};
