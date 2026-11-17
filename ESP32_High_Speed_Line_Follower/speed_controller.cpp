/**
 * speed_controller.cpp  -- see speed_controller.h
 */
#include "speed_controller.h"

SpeedController::SpeedController() {
  for (uint8_t i = 0; i < PREDICT_HISTORY; ++i) hist_[i] = 0.0f;
}

void SpeedController::begin() { reset(); }

void SpeedController::reset() {
  base_          = PWM_START;
  severity_      = 0.0f;
  predictActive_ = false;
  histIdx_ = 0;
  histLen_ = 0;
  for (uint8_t i = 0; i < PREDICT_HISTORY; ++i) hist_[i] = 0.0f;
}

void SpeedController::pushHistory_(float error) {
  hist_[histIdx_] = error;
  histIdx_ = (histIdx_ + 1) % PREDICT_HISTORY;
  if (histLen_ < PREDICT_HISTORY) histLen_++;
}

float SpeedController::predictSeverity_() const {
  // Lightweight trend: compare the magnitude of the most recent samples with
  // the older ones. A rising |error| trend predicts an approaching/sharpening
  // curve. Returns 0..1. This uses ONLY measured data -- it never claims to
  // see beyond the sensor's field of view.
  if (histLen_ < 4) return 0.0f;

  uint8_t half = histLen_ / 2;
  float recent = 0.0f, older = 0.0f;
  // Walk back from the newest sample.
  for (uint8_t k = 0; k < histLen_; ++k) {
    uint8_t idx = (histIdx_ + PREDICT_HISTORY - 1 - k) % PREDICT_HISTORY;
    float mag = fabsf(hist_[idx]);
    if (k < half) recent += mag; else older += mag;
  }
  recent /= half;
  older  /= (histLen_ - half);

  float rising = recent - older;          // >0 => error magnitude growing
  if (rising <= 0.0f) return 0.0f;
  float sev = rising * 3.0f;              // scale; tune empirically
  if (sev > 1.0f) sev = 1.0f;
  return sev;
}

uint8_t SpeedController::rampToward_(uint8_t current, uint8_t target) const {
  if (target > current) {
    uint16_t next = (uint16_t)current + BASE_ACCEL_STEP;
    return (next >= target) ? target : (uint8_t)next;
  } else if (target < current) {
    int16_t next = (int16_t)current - (int16_t)BASE_DECEL_STEP;
    return (next <= target) ? target : (uint8_t)next;
  }
  return current;
}

uint8_t SpeedController::computeBase(float error, float dError,
                                     uint16_t confidence) {
  pushHistory_(error);

  float absErr  = fabsf(error);
  float absDErr = fabsf(dError);

  // ---- Classify the section from measured error + its rate -------------
  //   severity 0 (straight) .. 1 (sharpest).
  float sevErr = 0.0f;
  if (absErr <= ERR_STRAIGHT_MAX) {
    sevErr = 0.0f;
  } else if (absErr >= ERR_SHARP_MIN) {
    sevErr = 1.0f;
  } else {
    sevErr = (absErr - ERR_STRAIGHT_MAX) / (ERR_SHARP_MIN - ERR_STRAIGHT_MAX);
  }

  // Rate-of-change contribution: a quickly moving line => developing curve.
  float sevRate = absDErr / DERR_SHARP;
  if (sevRate > 1.0f) sevRate = 1.0f;

  severity_ = max(sevErr, sevRate);

  // ---- Base target from severity ---------------------------------------
  // straight -> PWM_MAX ; sharp -> PWM_CURVE (linear blend).
  float target = (float)PWM_MAX - severity_ * (float)(PWM_MAX - PWM_CURVE);

  // ---- Predictive slow-down (optional) ---------------------------------
  predictActive_ = false;
  if (ENABLE_PREDICTIVE) {
    float psev = predictSeverity_();
    if (psev > 0.0f) {
      predictActive_ = true;
      target -= psev * (float)PREDICT_SLOWDOWN;
    }
  }

  // ---- Confidence cap ---------------------------------------------------
  // Low confidence => uncertain => do not run at full speed.
  if (confidence < LINE_CONFIDENCE_MIN * 2) {
    float capFrac = (float)confidence / (float)(LINE_CONFIDENCE_MIN * 2);
    if (capFrac < 0.4f) capFrac = 0.4f;   // never below 40% target
    target *= capFrac;
  }

  // ---- Clamp to legal band ---------------------------------------------
  if (target < PWM_MIN_MOVING) target = PWM_MIN_MOVING;
  if (target > PWM_MAX)        target = PWM_MAX;

  // ---- Rate-limit the base speed (smooth accel/decel) ------------------
  base_ = rampToward_(base_, (uint8_t)lroundf(target));
  return base_;
}
