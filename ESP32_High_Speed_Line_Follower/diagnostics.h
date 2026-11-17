/**
 * diagnostics.h
 * -----------------------------------------------------------------------------
 * Serial telemetry and debugging. All printing is THROTTLED and kept out of the
 * time-critical section. The control loop only *collects* data; diagnostics
 * prints it between cycles at TELEMETRY_PERIOD_MS.
 * -----------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "pid_controller.h"
#include "motor_control.h"
#include "speed_controller.h"
#include "race_manager.h"

class Diagnostics {
 public:
  Diagnostics();

  void begin();

  // Print a one-line telemetry record, but only if TELEMETRY_PERIOD_MS has
  // elapsed and debugging is enabled. Safe to call every loop.
  void maybeTelemetry(uint32_t nowMs,
                      const RaceManager& race,
                      const SensorManager& sensors,
                      const PIDController& pid,
                      const SpeedController& speed,
                      const MotorController& motors);

  // Unconditional detailed dumps (used by serial commands, outside the race).
  void dumpPID(const PIDController& pid) const;
  void dumpDiagnostics(uint32_t nowMs,
                       const RaceManager& race,
                       const SensorManager& sensors,
                       const PIDController& pid,
                       const MotorController& motors) const;
  void printHelp() const;

  // Throttled event log (line loss, etc.). Rate-limited to avoid flooding.
  void logEvent(const __FlashStringHelper* msg, uint32_t nowMs);

 private:
  uint32_t lastTelemetryMs_ = 0;
  uint32_t lastEventMs_     = 0;
};
