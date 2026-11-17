/**
 * pid_controller.cpp  -- see pid_controller.h
 */
#include "pid_controller.h"

PIDController::PIDController()
  : kp_(PID_KP), ki_(PID_KI), kd_(PID_KD) {}

void PIDController::begin() {
  kp_ = PID_KP; ki_ = PID_KI; kd_ = PID_KD;
  reset();
}

void PIDController::reset() {
  integral_  = 0.0f;
  prevError_ = 0.0f;
  filtDeriv_ = 0.0f;
  primed_    = false;
  pTerm_ = iTerm_ = dTerm_ = correction_ = 0.0f;
}

void PIDController::primeDerivative(float currentError) {
  // Make the next update() treat currentError as the baseline, so no spurious
  // derivative spike is produced from a stale previousError.
  prevError_ = currentError;
  filtDeriv_ = 0.0f;
  primed_    = true;
}

float PIDController::update(float error, float dt) {
  if (dt <= 0.0f)      dt = CONTROL_PERIOD_US / 1e6f;  // fallback nominal
  if (dt > DT_MAX_S)   dt = DT_MAX_S;                  // clamp after a stall

  // ---- Proportional -----------------------------------------------------
  pTerm_ = kp_ * error;

  // ---- Integral (with anti-windup; bypassed when Ki == 0) ---------------
  if (ki_ != 0.0f) {
    integral_ += error * dt;
    // Clamp the *contribution* (ki_*integral_) to +/- PID_I_LIMIT.
    float iContribMax = PID_I_LIMIT;
    float maxIntegral = (ki_ != 0.0f) ? (iContribMax / fabsf(ki_)) : 0.0f;
    if (integral_ >  maxIntegral) integral_ =  maxIntegral;
    if (integral_ < -maxIntegral) integral_ = -maxIntegral;
    iTerm_ = ki_ * integral_;
  } else {
    integral_ = 0.0f;
    iTerm_    = 0.0f;
  }

  // ---- Derivative (filtered) -------------------------------------------
  float rawDeriv = 0.0f;
  if (!primed_) {
    rawDeriv = (error - prevError_) / dt;
  } else {
    // First call after reset/prime: no derivative to avoid a kick.
    rawDeriv = 0.0f;
    primed_  = false;
  }
  // Exponential moving-average low-pass on the derivative.
  filtDeriv_ = PID_D_FILTER * filtDeriv_ + (1.0f - PID_D_FILTER) * rawDeriv;
  dTerm_     = kd_ * filtDeriv_;

  prevError_ = error;

  // ---- Sum + saturate ---------------------------------------------------
  float out = STEERING_SIGN * (pTerm_ + iTerm_ + dTerm_);
  if (out >  PID_CORRECTION_MAX) out =  PID_CORRECTION_MAX;
  if (out < -PID_CORRECTION_MAX) out = -PID_CORRECTION_MAX;

  correction_ = out;
  return out;
}
