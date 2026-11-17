/**
 * motor_control.cpp  -- see motor_control.h
 *
 * Uses the Arduino-ESP32 core 3.x LEDC API:
 *   ledcAttach(pin, freq, resolution_bits)  -> auto-assigns a channel
 *   ledcWrite(pin, duty)
 * These replace the pre-3.x ledcSetup()/ledcAttachPin()/ledcWrite(channel,..)
 * calls. See README for the supported board-version combination.
 */
#include "motor_control.h"

MotorController::MotorController() {}

void MotorController::begin() {
  // Direction pins.
  pinMode(MOTOR_L_IN1_PIN, OUTPUT);
  pinMode(MOTOR_L_IN2_PIN, OUTPUT);
  pinMode(MOTOR_R_IN1_PIN, OUTPUT);
  pinMode(MOTOR_R_IN2_PIN, OUTPUT);

  // PWM pins via LEDC (core 3.x). ledcAttach picks a free channel + timer.
  ledcAttach(MOTOR_L_PWM_PIN, MOTOR_PWM_FREQ_HZ, MOTOR_PWM_RES_BITS);
  ledcAttach(MOTOR_R_PWM_PIN, MOTOR_PWM_FREQ_HZ, MOTOR_PWM_RES_BITS);

  // Standby pin, if used.
  if (USE_STBY_PIN) {
    pinMode(MOTOR_STBY_PIN, OUTPUT);
    digitalWrite(MOTOR_STBY_PIN, LOW);   // start in standby (disabled)
  }

  // Start fully stopped and inhibited.
  tgtL_ = tgtR_ = outL_ = outR_ = 0;
  ledcWrite(MOTOR_L_PWM_PIN, 0);
  ledcWrite(MOTOR_R_PWM_PIN, 0);
  digitalWrite(MOTOR_L_IN1_PIN, LOW);
  digitalWrite(MOTOR_L_IN2_PIN, LOW);
  digitalWrite(MOTOR_R_IN1_PIN, LOW);
  digitalWrite(MOTOR_R_IN2_PIN, LOW);
  enabled_ = false;
}

void MotorController::enable() {
  if (USE_STBY_PIN) digitalWrite(MOTOR_STBY_PIN, HIGH);
  enabled_ = true;
}

void MotorController::disable() {
  // Coast and assert standby.
  ledcWrite(MOTOR_L_PWM_PIN, 0);
  ledcWrite(MOTOR_R_PWM_PIN, 0);
  digitalWrite(MOTOR_L_IN1_PIN, LOW);
  digitalWrite(MOTOR_L_IN2_PIN, LOW);
  digitalWrite(MOTOR_R_IN1_PIN, LOW);
  digitalWrite(MOTOR_R_IN2_PIN, LOW);
  if (USE_STBY_PIN) digitalWrite(MOTOR_STBY_PIN, LOW);
  enabled_ = false;
  tgtL_ = tgtR_ = outL_ = outR_ = 0;
}

int16_t MotorController::clampSpeed_(int32_t v) {
  if (v >  (int32_t)MOTOR_PWM_MAX) return  MOTOR_PWM_MAX;
  if (v < -(int32_t)MOTOR_PWM_MAX) return -MOTOR_PWM_MAX;
  return (int16_t)v;
}

void MotorController::setSpeeds(int16_t left, int16_t right) {
  tgtL_ = clampSpeed_(left);
  tgtR_ = clampSpeed_(right);
}

int16_t MotorController::slew_(int16_t current, int16_t target) {
  // Choose step based on whether the magnitude is increasing (accel) or
  // decreasing (decel). Compare magnitudes toward/away from zero.
  int16_t diff = target - current;
  if (diff == 0) return current;

  bool accelerating = (abs(target) > abs(current));
  int16_t step = accelerating ? (int16_t)MOTOR_ACCEL_STEP
                              : (int16_t)MOTOR_DECEL_STEP;
  if (diff >  step) return current + step;
  if (diff < -step) return current - step;
  return target;
}

void MotorController::writeMotor_(uint8_t in1, uint8_t in2, uint8_t pwmPin,
                                  int16_t signedSpeed, bool invert, float trim) {
  if (invert) signedSpeed = -signedSpeed;

  int32_t mag = (int32_t)lroundf(fabsf((float)signedSpeed) * trim);
  if (mag > MOTOR_PWM_MAX) mag = MOTOR_PWM_MAX;

  if (signedSpeed > 0) {            // forward
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else if (signedSpeed < 0) {     // reverse
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } else {                          // coast (both low)
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }
  ledcWrite(pwmPin, (uint32_t)mag);
}

void MotorController::apply() {
  // Advance outputs toward targets under slew limiting.
  outL_ = slew_(outL_, tgtL_);
  outR_ = slew_(outR_, tgtR_);

  if (!enabled_) {
    // Safety: never drive while inhibited.
    ledcWrite(MOTOR_L_PWM_PIN, 0);
    ledcWrite(MOTOR_R_PWM_PIN, 0);
    return;
  }

  writeMotor_(MOTOR_L_IN1_PIN, MOTOR_L_IN2_PIN, MOTOR_L_PWM_PIN,
              outL_, MOTOR_L_INVERT, MOTOR_L_TRIM);
  writeMotor_(MOTOR_R_IN1_PIN, MOTOR_R_IN2_PIN, MOTOR_R_PWM_PIN,
              outR_, MOTOR_R_INVERT, MOTOR_R_TRIM);
}

void MotorController::emergencyStop() {
  tgtL_ = tgtR_ = 0;
  outL_ = outR_ = 0;
  // Active short brake: both direction inputs HIGH, PWM high momentarily,
  // then settle to coast. For simplicity and safety we assert brake then zero.
  digitalWrite(MOTOR_L_IN1_PIN, HIGH);
  digitalWrite(MOTOR_L_IN2_PIN, HIGH);
  digitalWrite(MOTOR_R_IN1_PIN, HIGH);
  digitalWrite(MOTOR_R_IN2_PIN, HIGH);
  ledcWrite(MOTOR_L_PWM_PIN, 0);
  ledcWrite(MOTOR_R_PWM_PIN, 0);
}
