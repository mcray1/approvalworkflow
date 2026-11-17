/**
 * pid_controller.h
 * -----------------------------------------------------------------------------
 * PID steering controller operating entirely in NORMALIZED-error units
 * (error ~ [-1, +1]). Features:
 *   - measured-dt compensation (dt supplied per cycle, in seconds)
 *   - filtered derivative (EMA) to reject sensor noise
 *   - bounded integral with anti-windup (and optional Ki == 0 bypass)
 *   - correction saturation
 *   - runtime-adjustable gains
 *
 * Output is a steering "correction" in PWM counts, applied differentially:
 *   leftPWM  = base + correction
 *   rightPWM = base - correction
 * -----------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "config.h"

class PIDController {
 public:
  PIDController();

  void begin();

  // Compute steering correction (PWM counts) from the normalized error and the
  // measured elapsed time dt (seconds). dt is clamped internally to DT_MAX_S.
  float update(float error, float dt);

  // Clear integral/derivative state. Call on (re)start and after a long stall
  // or line re-acquisition to avoid a derivative "kick".
  void reset();

  // Suppress the next derivative contribution (used when resuming tracking
  // after recovery so the stale previousError does not cause a spike).
  void primeDerivative(float currentError);

  // ---- Runtime tuning ----------------------------------------------------
  void  setGains(float kp, float ki, float kd) { kp_ = kp; ki_ = ki; kd_ = kd; }
  void  setKp(float v) { kp_ = v; }
  void  setKi(float v) { ki_ = v; reset(); }   // reset integral on Ki change
  void  setKd(float v) { kd_ = v; }
  float getKp() const { return kp_; }
  float getKi() const { return ki_; }
  float getKd() const { return kd_; }

  // ---- Introspection for telemetry --------------------------------------
  float lastError()      const { return prevError_; }
  float lastP()          const { return pTerm_; }
  float lastI()          const { return iTerm_; }
  float lastD()          const { return dTerm_; }
  float lastCorrection() const { return correction_; }

 private:
  float kp_, ki_, kd_;

  float integral_    = 0.0f;
  float prevError_   = 0.0f;
  float filtDeriv_   = 0.0f;
  bool  primed_      = false;   // skip derivative on the first post-reset call

  // Last-computed terms (telemetry).
  float pTerm_ = 0, iTerm_ = 0, dTerm_ = 0, correction_ = 0;
};
