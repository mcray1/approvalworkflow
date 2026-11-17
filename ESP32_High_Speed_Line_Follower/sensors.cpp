/**
 * sensors.cpp
 * -----------------------------------------------------------------------------
 * Implementation of SensorManager.
 *
 * The time-critical acquire_() is the only place that depends on sensor
 * hardware. The default build uses the QTRSensors library in RC mode
 * (QTR-8RC). Analog and comparator-digital paths are stubbed with clearly
 * marked TODO sections so the controller above never changes.
 * -----------------------------------------------------------------------------
 */
#include "sensors.h"

// ---------------------------------------------------------------------------
//  Underlying driver instance (QTRSensors). Statically allocated.
// ---------------------------------------------------------------------------
#include <QTRSensors.h>
static QTRSensors qtr;
static uint16_t   qtrBuffer[SENSOR_COUNT];

SensorManager::SensorManager() {
  for (uint8_t i = 0; i < SENSOR_COUNT; ++i) { raw_[i] = 0; cal_[i] = 0; }
}

void SensorManager::begin() {
#if defined(SENSOR_TYPE_RC)
  qtr.setTypeRC();
#elif defined(SENSOR_TYPE_ANALOG)
  qtr.setTypeAnalog();
#else
  // Digital/comparator arrays are read directly in acquire_(); QTR not used.
#endif

#if defined(SENSOR_TYPE_RC) || defined(SENSOR_TYPE_ANALOG)
  qtr.setSensorPins(SENSOR_PINS, SENSOR_COUNT);
  qtr.setTimeout(SENSOR_RC_TIMEOUT_US);
  if (SENSOR_EMITTER_PIN != 0xFF) qtr.setEmitterPin(SENSOR_EMITTER_PIN);
#else
  for (uint8_t i = 0; i < SENSOR_COUNT; ++i) pinMode(SENSOR_PINS[i], INPUT);
#endif
  calibrated_ = false;
}

// ---------------------------------------------------------------------------
//  Calibration
// ---------------------------------------------------------------------------
void SensorManager::startCalibration() {
#if defined(SENSOR_TYPE_RC) || defined(SENSOR_TYPE_ANALOG)
  // Re-create fresh calibration storage by re-setting the pins.
  qtr.calibrate();  // first call allocates calibration arrays
#endif
  calibrated_ = false;
}

void SensorManager::calibrateStep() {
#if defined(SENSOR_TYPE_RC) || defined(SENSOR_TYPE_ANALOG)
  qtr.calibrate();
#else
  // Digital arrays need no analog calibration; nothing to do.
#endif
}

bool SensorManager::calibrationValid(uint8_t &failMask) const {
  failMask = 0;
#if defined(SENSOR_TYPE_RC) || defined(SENSOR_TYPE_ANALOG)
  if (qtr.calibrationOn.minimum == nullptr ||
      qtr.calibrationOn.maximum == nullptr) {
    failMask = 0xFF;
    return false;
  }
  bool ok = true;
  for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
    const uint16_t mn = qtr.calibrationOn.minimum[i];
    const uint16_t mx = qtr.calibrationOn.maximum[i];
    const uint16_t contrast = (mx > mn) ? (mx - mn) : 0;
    if (contrast < SENSOR_CONTRAST_MIN) { failMask |= (1 << i); ok = false; }
  }
  return ok;
#else
  // Comparator/digital: assume healthy (no analog contrast to check).
  return true;
#endif
}

// ---------------------------------------------------------------------------
//  Acquisition
// ---------------------------------------------------------------------------
void SensorManager::acquire_() {
#if defined(SENSOR_TYPE_RC) || defined(SENSOR_TYPE_ANALOG)
  // readCalibrated() returns 0..1000 per channel (0 = white/min reflect for
  // black-on-white after calibration). Using the "white line" variant when the
  // track is inverted keeps 1000 == "on the line".
  if (LINE_IS_BLACK) {
    qtr.readCalibrated(qtrBuffer, QTRReadMode::On);  // raw calibrated
  } else {
    qtr.readCalibrated(qtrBuffer, QTRReadMode::On);
  }
  for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
    uint8_t src = SENSOR_FLIP ? (SENSOR_COUNT - 1 - i) : i;
    uint16_t v = qtrBuffer[src];                 // 0..1000, high = dark
    // For a WHITE line on BLACK we invert so "on line" is still the high value.
    cal_[i] = LINE_IS_BLACK ? v : (uint16_t)(1000 - v);
    raw_[i] = cal_[i];                           // calibrated == exposed raw
  }
#else
  // Comparator/digital board: each pin reads HIGH on line (adjust per board).
  for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
    uint8_t src = SENSOR_FLIP ? (SENSOR_COUNT - 1 - i) : i;
    int bit = digitalRead(SENSOR_PINS[src]);
    bool onLine = LINE_IS_BLACK ? (bit == HIGH) : (bit == LOW);
    cal_[i] = onLine ? 1000 : 0;
    raw_[i] = cal_[i];
  }
#endif
}

bool SensorManager::computePosition_(uint16_t &pos, uint16_t &conf) const {
  uint32_t weighted = 0;
  uint32_t total    = 0;
  uint8_t  onLine    = 0;
  for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
    uint16_t v = cal_[i];
    weighted += (uint32_t)v * (uint32_t)(i * 1000);
    total    += v;
    if (v >= SENSOR_ON_LINE_MIN) onLine++;
  }
  conf = (total > 0xFFFF) ? 0xFFFF : (uint16_t)total;

  if (total < LINE_CONFIDENCE_MIN || onLine == 0) {
    // No reliable line this sample.
    return false;
  }
  pos = (uint16_t)(weighted / total);           // 0..POSITION_MAX
  return true;
}

void SensorManager::update() {
  uint32_t t0 = micros();
  acquire_();

  uint16_t pos  = position_;
  uint16_t conf = 0;
  bool     seen = computePosition_(pos, conf);
  confidence_   = conf;

  if (seen) {
    position_          = pos;
    lastValidPosition_ = pos;
    if (pos < POSITION_CENTER - 400)      lastDirection_ = -1; // line to left
    else if (pos > POSITION_CENTER + 400) lastDirection_ = +1; // line to right
    else                                   lastDirection_ = 0;

    foundCount_ = (foundCount_ < 255) ? foundCount_ + 1 : 255;
    lostCount_  = 0;
    if (foundCount_ >= LINE_FOUND_DEBOUNCE) lineDetected_ = true;
  } else {
    // Hold last position; debounce the loss.
    lostCount_ = (lostCount_ < 255) ? lostCount_ + 1 : 255;
    foundCount_ = 0;
    if (lostCount_ >= LINE_LOST_DEBOUNCE) lineDetected_ = false;
  }

  readMicros_ = micros() - t0;
}

float SensorManager::getError() const {
  // Normalized to roughly [-1, +1].
  return ((float)position_ - (float)POSITION_CENTER) / (float)POSITION_CENTER;
}

void SensorManager::printDiagnostics() const {
  Serial.print(F("[SENS] pos="));   Serial.print(position_);
  Serial.print(F(" conf="));        Serial.print(confidence_);
  Serial.print(F(" line="));        Serial.print(lineDetected_ ? 1 : 0);
  Serial.print(F(" dir="));         Serial.print(lastDirection_);
  Serial.print(F(" us="));          Serial.print(readMicros_);
  Serial.print(F(" cal=["));
  for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
    Serial.print(cal_[i]);
    if (i < SENSOR_COUNT - 1) Serial.print(',');
  }
  Serial.println(F("]"));
}
