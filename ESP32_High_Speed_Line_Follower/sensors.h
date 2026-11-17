/**
 * sensors.h
 * -----------------------------------------------------------------------------
 * Sensor management: initialization, calibration, acquisition, line-position
 * calculation, confidence estimation and line-loss detection.
 *
 * The public interface is deliberately independent of the underlying sensor
 * technology. The PID controller consumes only getPosition()/getError()/
 * isLineDetected()/getConfidence(); swapping QTR-8RC for QTR-8A (analog) or a
 * comparator-based digital array requires changes ONLY inside sensors.cpp.
 * -----------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "config.h"

class SensorManager {
 public:
  SensorManager();

  // Configure pins and the underlying driver. Call once from setup().
  void begin();

  // ---- Calibration -------------------------------------------------------
  // Clears any previous calibration.
  void startCalibration();
  // Take ONE calibration sample (non-blocking). Call repeatedly during the
  // calibration window while the user sweeps the array over black and white.
  void calibrateStep();
  // Validate that every channel achieved adequate contrast. Returns true on
  // success; fills failMask with a bit set per failing channel index.
  bool calibrationValid(uint8_t &failMask) const;

  // ---- Acquisition (time-critical) --------------------------------------
  // Reads the array, updates position/confidence/line-detected state and the
  // measured acquisition duration. Safe to call every control cycle.
  void update();

  // ---- Results -----------------------------------------------------------
  // Weighted line position, 0..POSITION_MAX (center = POSITION_CENTER).
  // When the line is lost this returns the LAST VALID position (held), so you
  // MUST gate on isLineDetected()/getConfidence() to tell a real reading from
  // a retained one.
  uint16_t getPosition()      const { return position_; }
  uint16_t getLastPosition()  const { return lastValidPosition_; }

  // Normalized error in [-~1, +~1]: (position - center)/center.
  float    getError()         const;

  // true while a reliable line is currently under the array (debounced).
  bool     isLineDetected()   const { return lineDetected_; }

  // Summed calibrated reflectance across channels (line "strength").
  uint16_t getConfidence()    const { return confidence_; }

  // Direction the line was last seen heading relative to center:
  //   -1 = line was to the LEFT, +1 = to the RIGHT, 0 = centered/unknown.
  int8_t   getLastDirection() const { return lastDirection_; }

  // Measured duration of the most recent update() read, microseconds.
  uint32_t getReadMicros()    const { return readMicros_; }

  // Raw and calibrated per-channel values (for diagnostics only).
  const uint16_t* rawValues()        const { return raw_; }
  const uint16_t* calibratedValues() const { return cal_; }

  // Human-readable diagnostic dump to Serial (NOT for the race loop).
  void printDiagnostics() const;

 private:
  uint16_t raw_[SENSOR_COUNT];
  uint16_t cal_[SENSOR_COUNT];

  uint16_t position_          = POSITION_CENTER;
  uint16_t lastValidPosition_ = POSITION_CENTER;
  uint16_t confidence_        = 0;
  bool     lineDetected_      = false;
  int8_t   lastDirection_     = 0;

  uint8_t  lostCount_         = 0;   // consecutive lost cycles (debounce)
  uint8_t  foundCount_        = 0;   // consecutive found cycles (debounce)
  uint32_t readMicros_        = 0;

  bool     calibrated_        = false;

  // Low-level, technology-specific read. Fills raw_[] and cal_[]. Implemented
  // in sensors.cpp per the selected SENSOR_TYPE_*.
  void acquire_();
  // Weighted-average position from cal_[] plus confidence. Returns false if no
  // reliable line (below confidence threshold) this sample.
  bool computePosition_(uint16_t &pos, uint16_t &conf) const;
};
