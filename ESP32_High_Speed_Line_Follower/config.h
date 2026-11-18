/**
 * config.h
 * -----------------------------------------------------------------------------
 * Centralized hardware mapping and tuning parameters for the ESP32 high-speed
 * line-following robot.
 *
 * EVERY pin, polarity, timing constant and tuning coefficient lives here so the
 * rest of the firmware never hard-codes a hardware detail. Change hardware or
 * tuning in this file only.
 *
 * Target toolchain : Arduino-ESP32 core 3.x (LEDC "v3" API)
 * Assumed hardware : ESP32 DevKit (classic ESP32), Pololu QTR-8RC (RC/digital),
 *                    Toshiba TB6612FNG dual motor driver.
 *
 * Units are documented next to every tunable value. Values marked
 * "CONFIRM" are assumptions for unspecified hardware and MUST be verified
 * before a final electrical/mechanical specification is issued.
 * -----------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>

// =============================================================================
//  1. SENSOR ARRAY
// =============================================================================

// Number of reflectance channels. QTR-8RC => 8.
constexpr uint8_t  SENSOR_COUNT        = 8;

// Sensor type selection. Only one should be enabled.
//   SENSOR_TYPE_RC     -> QTR-8RC   (digital discharge timing, default)
//   SENSOR_TYPE_ANALOG -> QTR-8A    (ADC, future support)
//   SENSOR_TYPE_DIGITAL-> comparator board returning 0/1 (future support)
#define SENSOR_TYPE_RC
// #define SENSOR_TYPE_ANALOG
// #define SENSOR_TYPE_DIGITAL

// GPIO assignment for the 8 sensors, left-to-right across the robot front.
// Index 0 is the LEFT-most sensor, index 7 the RIGHT-most. If your array is
// physically mirrored, either re-order this list or set SENSOR_FLIP below.
//
// These are the pins from the user's existing/working sketch. Sensor 8 uses
// GPIO4; the emitter (LEDON/CTRL) is NOT software-controlled here (see
// SENSOR_EMITTER_PIN) so GPIO4 is free to serve as a sensor channel.
constexpr uint8_t  SENSOR_PINS[SENSOR_COUNT] = {
  16, 17, 18, 19, 21, 22, 23, 4
};

// If the physical left/right orientation of the array is reversed relative to
// the wiring order above, set this true to flip the reported position in
// software (0 = left, 7000 = right is always preserved).
constexpr bool     SENSOR_FLIP         = false;

// Emitter control pin for the QTR board (LEDON / CTRL).
//   Left at 0xFF so the firmware does NOT drive the emitter. TIE THE QTR
//   LEDON/CTRL PIN HIGH (to 3.3 V) in hardware so the IR emitters stay on.
//   This is what frees GPIO4 above to be used as sensor 8 (the original sketch
//   wrongly used GPIO4 for both). If you later want software emitter control,
//   wire LEDON to a spare GPIO and set it here instead (must NOT be a pin
//   already in SENSOR_PINS or the motor/button pins).
constexpr uint8_t  SENSOR_EMITTER_PIN  = 0xFF;   // 0xFF = not used (tie high)

// RC discharge read timeout in microseconds. Larger = more sensitive but
// slower. 2500 us is the QTRSensors default; lower it if loop timing demands.
constexpr uint16_t SENSOR_RC_TIMEOUT_US = 2500;

// Line polarity. true  => BLACK line on WHITE background (track default)
//                false => WHITE line on BLACK background.
constexpr bool     LINE_IS_BLACK       = true;

// Line-position output range produced by the sensor module.
//   For N sensors: 0 .. (N-1)*1000 . Center = ((N-1)*1000)/2.
constexpr uint16_t POSITION_MAX        = (SENSOR_COUNT - 1) * 1000;  // 7000
constexpr uint16_t POSITION_CENTER     = POSITION_MAX / 2;           // 3500

// =============================================================================
//  2. SENSOR CONFIDENCE / LINE-LOSS DETECTION
// =============================================================================

// Minimum summed calibrated reflectance (across all channels) that counts as
// "a line is present". Calibrated channel readings are 0..1000. If the total
// is below this, no sensor sees meaningful contrast -> line considered lost.
// Tune after calibration (see CALIBRATION.md). CONFIRM on real track.
constexpr uint16_t LINE_CONFIDENCE_MIN = 350;    // sum of calibrated values

// A single channel must exceed this calibrated value to be "on the line".
constexpr uint16_t SENSOR_ON_LINE_MIN  = 200;    // 0..1000

// Minimum contrast (calibrated max-min per channel) required at calibration
// time for the channel to be considered healthy.
constexpr uint16_t SENSOR_CONTRAST_MIN = 300;    // raw counts

// Debounce: number of consecutive control cycles the line must be absent
// before we declare it lost (rejects single-sample noise). At 200 Hz,
// 3 cycles = 15 ms.
constexpr uint8_t  LINE_LOST_DEBOUNCE  = 3;

// Debounce: consecutive cycles the line must be seen again before we declare
// it re-acquired during recovery.
constexpr uint8_t  LINE_FOUND_DEBOUNCE = 3;

// =============================================================================
//  3. MOTOR DRIVER (TB6612FNG)
// =============================================================================

// Left motor (channel A)  -- pins from the user's existing/working sketch.
constexpr uint8_t  MOTOR_L_IN1_PIN     = 26;     // AIN1
constexpr uint8_t  MOTOR_L_IN2_PIN     = 27;     // AIN2
constexpr uint8_t  MOTOR_L_PWM_PIN     = 25;     // PWMA

// Right motor (channel B)
// NOTE: BIN1 is on GPIO12, an ESP32 strapping pin (flash-voltage select). This
// matches the user's working wiring and boots fine on boards where GPIO12 is
// low at reset. If you ever see boot failures, move BIN1 to e.g. GPIO33.
constexpr uint8_t  MOTOR_R_IN1_PIN     = 12;     // BIN1  (strapping pin, see note)
constexpr uint8_t  MOTOR_R_IN2_PIN     = 13;     // BIN2
constexpr uint8_t  MOTOR_R_PWM_PIN     = 14;     // PWMB

// Standby control. If USE_STBY_PIN is false, tie TB6612 STBY to 3.3V in
// hardware. If true, MOTOR_STBY_PIN drives STBY (HIGH = enabled).
constexpr bool     USE_STBY_PIN        = false;
constexpr uint8_t  MOTOR_STBY_PIN      = 15;     // used only if USE_STBY_PIN

// Motor polarity inversion. If a motor spins the wrong way for "forward",
// flip its flag instead of swapping wires.
constexpr bool     MOTOR_L_INVERT      = false;
constexpr bool     MOTOR_R_INVERT      = false;

// Left/right trim to compensate for mechanical/motor mismatch.
// Final PWM is multiplied by these. 1.0 = no change. Keep within ~0.85..1.0.
// CONFIRM by the straight-line test in TEST_PLAN.md.
constexpr float    MOTOR_L_TRIM        = 1.00f;
constexpr float    MOTOR_R_TRIM        = 1.00f;

// PWM configuration (ESP32 LEDC).
//   20 kHz keeps switching above the audible range and suits small brushed
//   motors + TB6612. 8-bit resolution gives the required 0..255 range.
constexpr uint32_t MOTOR_PWM_FREQ_HZ   = 20000;  // CONFIRM for your motors
constexpr uint8_t  MOTOR_PWM_RES_BITS  = 8;      // 0..255
constexpr uint16_t MOTOR_PWM_MAX       = (1 << MOTOR_PWM_RES_BITS) - 1; // 255

// Acceleration limiting (slew rate). Maximum change in commanded PWM per
// control cycle, per motor. Prevents wheelspin and current spikes.
// At 200 Hz, 12 counts/cycle => ~0 to 255 in ~106 ms.
constexpr uint8_t  MOTOR_ACCEL_STEP    = 12;     // PWM counts / cycle (speed up)
constexpr uint8_t  MOTOR_DECEL_STEP    = 25;     // PWM counts / cycle (slow down)

// =============================================================================
//  4. CONTROL-LOOP TIMING
// =============================================================================

// Target control period. 5000 us = 5 ms = 200 Hz.
constexpr uint32_t CONTROL_PERIOD_US   = 5000;
constexpr float    CONTROL_FREQ_HZ     = 1000000.0f / CONTROL_PERIOD_US; // 200

// If a measured cycle exceeds this, it is counted as a missed deadline.
constexpr uint32_t DEADLINE_WARN_US    = CONTROL_PERIOD_US + (CONTROL_PERIOD_US / 5); // +20%

// Clamp for the dt used by the PID when a cycle runs long (keeps derivative/
// integral sane after a stall). In seconds.
constexpr float    DT_MAX_S            = 0.050f; // 50 ms

// =============================================================================
//  5. PID STEERING (normalized-error units)
// =============================================================================
//  error is normalized to roughly [-1, +1]:  error = (position-3500)/3500.
//  ALL gains below are expressed for that normalized error. Do NOT mix with
//  raw-position units.

constexpr float    PID_KP              = 30.0f;  // provisional
constexpr float    PID_KI              = 0.0f;   // start at 0 (often unused)
constexpr float    PID_KD              = 0.30f;  // provisional

// Derivative low-pass filter coefficient (EMA). 0 = no filtering (raw),
// approaching 1 = heavy smoothing. 0.6..0.8 is a typical start.
constexpr float    PID_D_FILTER        = 0.70f;

// Integral anti-windup clamp (absolute value of the integral term's
// contribution, in PWM counts once multiplied by Ki). Ignored when Ki == 0.
constexpr float    PID_I_LIMIT         = 40.0f;

// Correction saturation. Maximum |correction| applied to the differential
// drive, in PWM counts. Limits steering authority so it cannot exceed what the
// motors can deliver and prevents violent reversals.
constexpr float    PID_CORRECTION_MAX  = 255.0f;

// Steering sign. If PID steers AWAY from the line (Test 5 fails), set to -1.
constexpr float    STEERING_SIGN       = +1.0f;

// =============================================================================
//  6. SPEED PROFILE
// =============================================================================

constexpr uint8_t  PWM_START           = 100;    // launch base PWM
constexpr uint8_t  PWM_MAX             = 200;    // max racing base PWM
constexpr uint8_t  PWM_CURVE           = 100;    // base PWM in sharp curves (80..120)
constexpr uint8_t  PWM_MIN_MOVING      = 60;     // floor so the robot keeps moving

// Adaptive-speed ramp rates for the BASE speed target (counts / cycle).
constexpr uint8_t  BASE_ACCEL_STEP     = 3;      // ramp base speed up
constexpr uint8_t  BASE_DECEL_STEP     = 10;     // ramp base speed down

// Error magnitudes (normalized, 0..~1) that classify the track section.
//   |error| below STRAIGHT => treat as straight (allow acceleration)
//   |error| above SHARP    => treat as sharp curve (reduce speed)
constexpr float    ERR_STRAIGHT_MAX    = 0.15f;
constexpr float    ERR_SHARP_MIN       = 0.45f;

// Derivative (rate of change of error, 1/s) above this flags a fast-developing
// curve for predictive slow-down.
constexpr float    DERR_SHARP          = 4.0f;

// =============================================================================
//  7. PREDICTIVE CORNERING (optional)
// =============================================================================

// Master switch so predictive behavior can be compared against plain PID.
constexpr bool     ENABLE_PREDICTIVE   = true;

// Number of recent error samples kept for trend analysis. Keep small for the
// 5 ms loop. Fixed-size, statically allocated.
constexpr uint8_t  PREDICT_HISTORY     = 8;

// Extra base-speed reduction (PWM counts) applied when the trend predicts an
// upcoming sharpening of the turn. Scaled by predicted severity 0..1.
constexpr uint8_t  PREDICT_SLOWDOWN    = 40;

// =============================================================================
//  8. LINE-LOSS RECOVERY
// =============================================================================

constexpr uint8_t  PWM_RECOVERY        = 85;     // search base PWM (70..100)
constexpr uint32_t RECOVERY_TIMEOUT_MS = 1200;   // hard stop if not reacquired

// Differential split during the search sweep, as a fraction of PWM_RECOVERY
// applied to the two wheels (inner slows, outer speeds). 1.0 = pivot hard.
constexpr float    RECOVERY_TURN_RATIO = 1.0f;

// =============================================================================
//  9. USER INTERFACE / BUTTON
// =============================================================================

// Start/Stop button. GPIO34 is INPUT-ONLY with NO internal pull-up: an
// external 10k pull-up to 3.3V is REQUIRED and the button connects to GND
// (pressed = LOW).
constexpr uint8_t  BUTTON_PIN          = 34;
constexpr bool     BUTTON_ACTIVE_LOW   = true;   // pressed drives the pin LOW
constexpr uint16_t BUTTON_DEBOUNCE_MS  = 40;

// =============================================================================
//  10. CALIBRATION
// =============================================================================

// Total time spent sampling during calibration. The user sweeps the array over
// black and white during this window. ~4.5 s.
constexpr uint32_t CALIBRATION_TIME_MS = 4500;

// How often to take a calibration sample during that window.
constexpr uint16_t CALIBRATION_STEP_MS = 20;

// =============================================================================
//  11. DIAGNOSTICS / TELEMETRY
// =============================================================================

// Master debug switch. Set false (or build with COMPETITION_MODE) to silence
// everything non-essential for racing.
constexpr bool     DEBUG_ENABLED       = true;

// When true, overrides DEBUG_ENABLED to minimum output for competition.
#define COMPETITION_MODE_OFF
#ifdef COMPETITION_MODE_ON
  constexpr bool   COMPETITION_MODE     = true;
#else
  constexpr bool   COMPETITION_MODE     = false;
#endif

constexpr uint32_t SERIAL_BAUD         = 115200;

// Minimum interval between telemetry prints while RUNNING (ms). Keeps Serial
// out of the time-critical path. Telemetry is NEVER printed every cycle.
constexpr uint16_t TELEMETRY_PERIOD_MS = 200;

// =============================================================================
//  12. OPTIONAL FEATURES (disabled by default; see section-12 docs)
// =============================================================================

// Finish-line detection. Disabled until marker geometry + rules are defined.
constexpr bool     ENABLE_FINISH_LINE  = false;

// Battery monitoring via ADC voltage divider. Disabled until divider + pin are
// confirmed safe for the ESP32 (<= 3.3 V at the pin).
constexpr bool     ENABLE_BATTERY_MON  = false;
constexpr uint8_t  BATTERY_ADC_PIN     = 35;     // input-only ADC1 pin, CONFIRM
constexpr float    BATTERY_DIVIDER     = 11.0f;  // (R1+R2)/R2, CONFIRM
constexpr float    BATTERY_WARN_V      = 7.0f;   // CONFIRM for your pack
constexpr float    BATTERY_STOP_V      = 6.6f;   // CONFIRM for your pack

// Wheel encoders. Interface reserved only; not required for base operation.
constexpr bool     ENABLE_ENCODERS     = false;
