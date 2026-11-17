/**
 * ESP32_High_Speed_Line_Follower.ino
 * =============================================================================
 * High-speed autonomous line-following racing robot - main application.
 *
 * Target     : ESP32 DevKit (classic ESP32), Arduino-ESP32 core 3.x
 * Sensors    : QTR-8RC (8-channel RC reflectance array)  [QTRSensors library]
 * Driver     : TB6612FNG dual DC motor driver
 *
 * Architecture: a deterministic 5 ms (200 Hz) control loop driven by micros(),
 * a finite-state machine (BOOT -> CALIBRATING -> READY -> RUNNING <-> RECOVERING
 * -> STOPPED/ERROR, optional FINISHED), normalized-error PID steering, adaptive
 * speed control, and bounded line-loss recovery.
 *
 * SAFETY: motors are inhibited at boot and after every stop. Racing only begins
 * on an explicit start command (button in READY, or 's' over Serial). A reboot
 * NEVER auto-starts the motors.
 *
 * BUILD STATUS: This firmware has NOT been compiled or run on hardware in this
 * environment. Treat the build as UNVERIFIED until you compile it against the
 * library/board versions listed in README.md and run the TEST_PLAN.md checks.
 * =============================================================================
 */
#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "pid_controller.h"
#include "motor_control.h"
#include "speed_controller.h"
#include "line_recovery.h"
#include "race_manager.h"
#include "diagnostics.h"

// ---------------------------------------------------------------------------
//  Module instances (all statically allocated - no heap in the control path).
// ---------------------------------------------------------------------------
static SensorManager  sensors;
static PIDController   pid;
static MotorController motors;
static SpeedController speed;
static LineRecovery    recovery;
static RaceManager     race;
static Diagnostics     diag;

// ---------------------------------------------------------------------------
//  Loop-timing / scheduler state.
// ---------------------------------------------------------------------------
static uint32_t lastControlUs = 0;

// Local error-derivative filter for the speed controller (separate from the
// PID's internal derivative, expressed in normalized-error units per second).
static float prevErrorForD = 0.0f;
static float filtDErr      = 0.0f;
static bool  dPrimed       = false;

// Calibration windowing.
static uint32_t calStartMs   = 0;
static uint32_t lastCalStepMs = 0;

// Button debounce state.
static bool     btnStable    = false;  // debounced "pressed" state
static bool     btnLastRead  = false;
static uint32_t btnChangeMs  = 0;

// Serial line buffer for command parsing.
static char     cmdBuf[16];
static uint8_t  cmdLen = 0;

// ---------------------------------------------------------------------------
//  Forward declarations.
// ---------------------------------------------------------------------------
static void enterCalibrating(uint32_t nowMs);
static void enterReady(uint32_t nowMs);
static void enterRunning(uint32_t nowMs);
static void enterRecovering(uint32_t nowMs);
static void enterStopped(uint32_t nowMs, const __FlashStringHelper* why);
static void enterError(uint32_t nowMs, const __FlashStringHelper* why);

static void serviceSerial(uint32_t nowMs);
static void serviceButton(uint32_t nowMs);
static void controlCycle(uint32_t periodUs, uint32_t nowMs);
static void runningCycle(float dtS, uint32_t nowMs);
static void recoveringCycle(uint32_t nowMs);
static bool detectFinish();   // optional, disabled by default

// ===========================================================================
//  setup()
// ===========================================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(50);
  Serial.println();
  Serial.println(F("=== ESP32 High-Speed Line Follower ==="));
  Serial.println(F("BUILD: unverified until compiled on your toolchain."));

  // Button: GPIO34 is input-only, no internal pull-up -> external 10k to 3.3V.
  pinMode(BUTTON_PIN, INPUT);

  sensors.begin();
  motors.begin();      // leaves motors DISABLED (inhibited)
  pid.begin();
  speed.begin();
  diag.begin();

  race.setState(RaceState::BOOT, millis());
  diag.printHelp();

  // Begin calibration automatically; motors stay inhibited throughout.
  enterCalibrating(millis());
}

// ===========================================================================
//  loop()  -- non-blocking; the 5 ms control work is gated by micros().
// ===========================================================================
void loop() {
  uint32_t nowMs = millis();

  // 1) Always service the UI (cheap, non-blocking).
  serviceSerial(nowMs);
  serviceButton(nowMs);

  // 2) Calibration progresses in its own non-blocking window.
  if (race.is(RaceState::CALIBRATING)) {
    if (nowMs - lastCalStepMs >= CALIBRATION_STEP_MS) {
      lastCalStepMs = nowMs;
      sensors.calibrateStep();
    }
    if (nowMs - calStartMs >= CALIBRATION_TIME_MS) {
      uint8_t failMask = 0;
      if (sensors.calibrationValid(failMask)) {
        Serial.println(F("[CAL] OK"));
        enterReady(nowMs);
      } else {
        Serial.print(F("[CAL] FAILED. Weak channels mask=0x"));
        Serial.println(failMask, HEX);
        Serial.println(F("[CAL] Press button or send 'c' to recalibrate."));
        enterError(nowMs, F("calibration contrast too low"));
      }
    }
    return;  // nothing else runs during calibration
  }

  // 3) Deterministic control loop at CONTROL_PERIOD_US.
  uint32_t nowUs = micros();
  if ((uint32_t)(nowUs - lastControlUs) >= CONTROL_PERIOD_US) {
    uint32_t periodUs = nowUs - lastControlUs;
    lastControlUs = nowUs;               // rollover-safe unsigned arithmetic
    controlCycle(periodUs, nowMs);
  }

  // 4) Telemetry between cycles (throttled; never in the hot path).
  diag.maybeTelemetry(nowMs, race, sensors, pid, speed, motors);
}

// ===========================================================================
//  Control cycle dispatch
// ===========================================================================
static void controlCycle(uint32_t periodUs, uint32_t nowMs) {
  uint32_t t0 = micros();

  // Clamp dt for sanity (seconds).
  float dtS = (float)periodUs / 1e6f;
  if (dtS > DT_MAX_S) dtS = DT_MAX_S;

  switch (race.state()) {
    case RaceState::RUNNING:
      runningCycle(dtS, nowMs);
      break;
    case RaceState::RECOVERING:
      recoveringCycle(nowMs);
      break;
    case RaceState::READY:
    case RaceState::STOPPED:
    case RaceState::ERROR:
    case RaceState::FINISHED:
    default:
      // Motors inhibited in all non-driving states. Keep outputs at zero.
      motors.setSpeeds(0, 0);
      motors.apply();
      break;
  }

  uint32_t computeUs = micros() - t0;
  race.stats.note(periodUs, computeUs);
}

// ===========================================================================
//  RUNNING: normalized-error PID + adaptive speed
// ===========================================================================
static void runningCycle(float dtS, uint32_t nowMs) {
  sensors.update();

  if (!sensors.isLineDetected()) {
    // Line lost -> begin bounded recovery.
    race.noteLineLoss();
    diag.logEvent(F("line lost -> RECOVERING"), nowMs);
    enterRecovering(nowMs);
    return;
  }

  float error = sensors.getError();     // normalized [-~1, +1]

  // Local filtered derivative (1/s) for the speed controller.
  float rawD = dPrimed ? 0.0f : (error - prevErrorForD) / dtS;
  dPrimed = false;
  filtDErr = PID_D_FILTER * filtDErr + (1.0f - PID_D_FILTER) * rawD;
  prevErrorForD = error;

  // PID steering correction (PWM counts).
  float correction = pid.update(error, dtS);

  // Adaptive base speed (PWM counts).
  uint8_t base = speed.computeBase(error, filtDErr, sensors.getConfidence());

  // Differential drive.
  int16_t leftPWM  = (int16_t)base + (int16_t)lroundf(correction);
  int16_t rightPWM = (int16_t)base - (int16_t)lroundf(correction);

  // Saturate to the legal motor range (setSpeeds also clamps).
  motors.setSpeeds(leftPWM, rightPWM);
  motors.apply();

  // Optional finish-line detection (disabled by default).
  if (ENABLE_FINISH_LINE && detectFinish()) {
    diag.logEvent(F("finish detected"), nowMs);
    motors.emergencyStop();
    motors.disable();
    race.setState(RaceState::FINISHED, nowMs);
    Serial.print(F("[FINISH] lap time = "));
    Serial.print(race.raceElapsedMs(nowMs));
    Serial.println(F(" ms"));
  }
}

// ===========================================================================
//  RECOVERING: bounded search for the lost line
// ===========================================================================
static void recoveringCycle(uint32_t nowMs) {
  sensors.update();

  int16_t outL = 0, outR = 0;
  RecoveryStatus st = recovery.update(sensors.isLineDetected(), nowMs,
                                      outL, outR);

  switch (st) {
    case RecoveryStatus::SEARCHING:
      motors.setSpeeds(outL, outR);
      motors.apply();
      break;

    case RecoveryStatus::REACQUIRED:
      race.noteRecovery();
      diag.logEvent(F("line reacquired -> RUNNING"), nowMs);
      // Avoid derivative kick: prime both the PID and the local derivative
      // with the current error before resuming.
      {
        float e = sensors.getError();
        pid.primeDerivative(e);
        prevErrorForD = e;
        filtDErr      = 0.0f;
        dPrimed       = true;
      }
      // Resume from a modest base so acceleration is gradual after recovery.
      speed.reset();
      race.setState(RaceState::RUNNING, nowMs);
      break;

    case RecoveryStatus::TIMED_OUT:
      diag.logEvent(F("recovery timeout -> STOPPED"), nowMs);
      enterStopped(nowMs, F("recovery timed out"));
      break;
  }
}

// ===========================================================================
//  State-entry helpers (explicit transitions)
// ===========================================================================
static void enterCalibrating(uint32_t nowMs) {
  motors.disable();                 // inhibit motors during calibration
  motors.setSpeeds(0, 0);
  sensors.startCalibration();
  calStartMs    = nowMs;
  lastCalStepMs = nowMs;
  race.setState(RaceState::CALIBRATING, nowMs);
  Serial.println(F("[CAL] Sweep the array over BLACK and WHITE now (~4.5 s)."));
}

static void enterReady(uint32_t nowMs) {
  motors.disable();
  motors.setSpeeds(0, 0);
  race.setState(RaceState::READY, nowMs);
  Serial.println(F("[READY] Press button or send 's' to start."));
}

static void enterRunning(uint32_t nowMs) {
  // Reset controllers for a clean launch.
  pid.reset();
  speed.reset();
  prevErrorForD = sensors.getError();
  filtDErr      = 0.0f;
  dPrimed       = true;
  lastControlUs = micros();

  race.resetCounters();
  race.markRaceStart(nowMs);
  motors.enable();                  // release the driver standby
  race.setState(RaceState::RUNNING, nowMs);
  Serial.println(F("[RUN] Race started."));
}

static void enterRecovering(uint32_t nowMs) {
  recovery.begin(sensors.getLastDirection(), nowMs);
  race.setState(RaceState::RECOVERING, nowMs);
}

static void enterStopped(uint32_t nowMs, const __FlashStringHelper* why) {
  motors.emergencyStop();
  motors.disable();
  race.setState(RaceState::STOPPED, nowMs);
  Serial.print(F("[STOP] ")); Serial.println(why);
  Serial.println(F("[STOP] Send 's' (from READY) or 'c' to recalibrate."));
}

static void enterError(uint32_t nowMs, const __FlashStringHelper* why) {
  motors.emergencyStop();
  motors.disable();
  race.setState(RaceState::ERROR, nowMs);
  Serial.print(F("[ERROR] ")); Serial.println(why);
}

// ===========================================================================
//  User interface: button (debounced) and Serial commands
// ===========================================================================
static void serviceButton(uint32_t nowMs) {
  bool raw = digitalRead(BUTTON_PIN) == (BUTTON_ACTIVE_LOW ? LOW : HIGH);

  if (raw != btnLastRead) {         // input changed -> restart debounce timer
    btnLastRead = raw;
    btnChangeMs = nowMs;
  } else if ((nowMs - btnChangeMs) >= BUTTON_DEBOUNCE_MS && raw != btnStable) {
    // Stable for the debounce interval -> accept new state.
    btnStable = raw;
    if (btnStable) {                // rising edge of a debounced PRESS
      switch (race.state()) {
        case RaceState::READY:
          enterRunning(nowMs);
          break;
        case RaceState::RUNNING:
        case RaceState::RECOVERING:
          enterStopped(nowMs, F("button stop"));
          break;
        case RaceState::STOPPED:
        case RaceState::ERROR:
        case RaceState::FINISHED:
          // From a halted state the button triggers a fresh calibration so the
          // robot can never leap straight back into motion unexpectedly.
          enterCalibrating(nowMs);
          break;
        default:
          break;
      }
    }
  }
}

static void applyTuning(char which, const char* numStr) {
  float v = atof(numStr);
  // Reject obviously invalid input (atof returns 0.0 on garbage; guard ranges).
  switch (which) {
    case 'P':
      if (v >= 0.0f && v <= 200.0f) { pid.setKp(v); Serial.print(F("Kp=")); Serial.println(v, 3); }
      else Serial.println(F("[ERR] Kp out of range [0,200]"));
      break;
    case 'I':
      if (v >= 0.0f && v <= 50.0f)  { pid.setKi(v); Serial.print(F("Ki=")); Serial.println(v, 3); }
      else Serial.println(F("[ERR] Ki out of range [0,50]"));
      break;
    case 'D':
      if (v >= 0.0f && v <= 50.0f)  { pid.setKd(v); Serial.print(F("Kd=")); Serial.println(v, 3); }
      else Serial.println(F("[ERR] Kd out of range [0,50]"));
      break;
    default:
      Serial.println(F("[ERR] unknown tuning key"));
      break;
  }
}

static void handleCommandChar(char c, uint32_t nowMs) {
  switch (c) {
    case 's':
      if (race.is(RaceState::READY)) enterRunning(nowMs);
      else Serial.println(F("[ERR] 's' only from READY (recalibrate with 'c')."));
      break;
    case 'x':
      enterStopped(nowMs, F("serial stop"));
      break;
    case 'c':
      enterCalibrating(nowMs);
      break;
    case 'p':
      diag.dumpPID(pid);
      break;
    case 'd':
      diag.dumpDiagnostics(nowMs, race, sensors, pid, motors);
      break;
    case 'h':
      diag.printHelp();
      break;
    default:
      Serial.println(F("[ERR] unknown command. 'h' for help."));
      break;
  }
}

static void serviceSerial(uint32_t nowMs) {
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      if (cmdLen > 0) {
        cmdBuf[cmdLen] = '\0';
        if (cmdBuf[0] == 'P' || cmdBuf[0] == 'I' || cmdBuf[0] == 'D') {
          applyTuning(cmdBuf[0], cmdBuf + 1);
        } else {
          handleCommandChar(cmdBuf[0], nowMs);
        }
        cmdLen = 0;
      }
      continue;
    }

    // Accumulate.
    if (cmdLen < sizeof(cmdBuf) - 1) cmdBuf[cmdLen++] = c;

    // Immediate execution for single-letter lowercase commands (no Enter
    // needed), while uppercase P/I/D keep buffering their numeric argument.
    if (cmdLen == 1) {
      if (c == 's' || c == 'x' || c == 'c' ||
          c == 'p' || c == 'd' || c == 'h') {
        handleCommandChar(c, nowMs);
        cmdLen = 0;
      }
    }
  }
}

// ===========================================================================
//  Optional finish-line detector (disabled by default).
//  A real implementation must define the finish marker geometry (e.g. a wide
//  band spanning ALL sensors for a minimum dwell time) and debounce it so that
//  intersections and wide black regions are not misread. Left as a safe stub.
// ===========================================================================
static bool detectFinish() {
  // Example skeleton (NOT enabled): require every channel dark for N cycles.
  // Define and validate marker rules before turning ENABLE_FINISH_LINE on.
  return false;
}
