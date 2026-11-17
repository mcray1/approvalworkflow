/**
 * diagnostics.cpp  -- see diagnostics.h
 */
#include "diagnostics.h"

Diagnostics::Diagnostics() {}

void Diagnostics::begin() {}

void Diagnostics::maybeTelemetry(uint32_t nowMs,
                                 const RaceManager& race,
                                 const SensorManager& sensors,
                                 const PIDController& pid,
                                 const SpeedController& speed,
                                 const MotorController& motors) {
  if (!DEBUG_ENABLED || COMPETITION_MODE) return;
  if (nowMs - lastTelemetryMs_ < TELEMETRY_PERIOD_MS) return;
  lastTelemetryMs_ = nowMs;

  // Compact single-line record. Keep field count modest for 115200 baud.
  Serial.print(F("["));   Serial.print(raceStateName(race.state()));
  Serial.print(F("] pos="));  Serial.print(sensors.getPosition());
  Serial.print(F(" err="));   Serial.print(sensors.getError(), 3);
  Serial.print(F(" conf="));  Serial.print(sensors.getConfidence());
  Serial.print(F(" line="));  Serial.print(sensors.isLineDetected() ? 1 : 0);
  Serial.print(F(" corr="));  Serial.print(pid.lastCorrection(), 1);
  Serial.print(F(" base="));  Serial.print(speed.base());
  Serial.print(F(" sev="));   Serial.print(speed.lastSeverity(), 2);
  Serial.print(F(" L="));     Serial.print(motors.outputLeft());
  Serial.print(F(" R="));     Serial.print(motors.outputRight());
  Serial.print(F(" loopUs=")); Serial.print(race.stats.lastPeriodUs);
  Serial.print(F(" miss="));  Serial.print(race.stats.missedDeadlines);
  Serial.println();
}

void Diagnostics::dumpPID(const PIDController& pid) const {
  Serial.println(F("---- PID parameters ----"));
  Serial.print(F("Kp = ")); Serial.println(pid.getKp(), 4);
  Serial.print(F("Ki = ")); Serial.println(pid.getKi(), 4);
  Serial.print(F("Kd = ")); Serial.println(pid.getKd(), 4);
  Serial.print(F("P  = ")); Serial.println(pid.lastP(), 3);
  Serial.print(F("I  = ")); Serial.println(pid.lastI(), 3);
  Serial.print(F("D  = ")); Serial.println(pid.lastD(), 3);
  Serial.print(F("corr=")); Serial.println(pid.lastCorrection(), 3);
  Serial.println(F("(units: normalized error, correction in PWM counts)"));
}

void Diagnostics::dumpDiagnostics(uint32_t nowMs,
                                  const RaceManager& race,
                                  const SensorManager& sensors,
                                  const PIDController& pid,
                                  const MotorController& motors) const {
  Serial.println(F("======== DIAGNOSTICS ========"));
  Serial.print(F("State        : ")); Serial.println(raceStateName(race.state()));
  Serial.print(F("Race elapsed : ")); Serial.print(race.raceElapsedMs(nowMs));
  Serial.println(F(" ms"));
  Serial.print(F("Line losses  : ")); Serial.println(race.lineLossCount());
  Serial.print(F("Recoveries   : ")); Serial.println(race.recoveryCount());
  Serial.println(F("---- Timing ----"));
  Serial.print(F("loop period  : ")); Serial.print(race.stats.lastPeriodUs);
  Serial.print(F(" us (max "));       Serial.print(race.stats.maxPeriodUs);
  Serial.println(F(" us)"));
  Serial.print(F("compute      : ")); Serial.print(race.stats.lastComputeUs);
  Serial.print(F(" us (max "));       Serial.print(race.stats.maxComputeUs);
  Serial.println(F(" us)"));
  Serial.print(F("sensor read  : ")); Serial.print(sensors.getReadMicros());
  Serial.println(F(" us"));
  Serial.print(F("missed dl    : ")); Serial.print(race.stats.missedDeadlines);
  Serial.print(F(" / "));             Serial.print(race.stats.cycles);
  Serial.println(F(" cycles"));
  Serial.println(F("---- Sensors ----"));
  sensors.printDiagnostics();
  Serial.println(F("---- Motors ----"));
  Serial.print(F("target L/R   : ")); Serial.print(motors.targetLeft());
  Serial.print('/');                  Serial.println(motors.targetRight());
  Serial.print(F("output L/R   : ")); Serial.print(motors.outputLeft());
  Serial.print('/');                  Serial.println(motors.outputRight());
  Serial.print(F("enabled      : ")); Serial.println(motors.isEnabled() ? 1 : 0);
  dumpPID(pid);
  Serial.println(F("============================="));
}

void Diagnostics::printHelp() const {
  Serial.println(F("==== Serial commands ===="));
  Serial.println(F("  s  start race (only from READY)"));
  Serial.println(F("  x  stop immediately"));
  Serial.println(F("  c  recalibrate sensors"));
  Serial.println(F("  p  print PID parameters"));
  Serial.println(F("  d  print diagnostics"));
  Serial.println(F("  h  this help"));
  Serial.println(F("  Tuning (optional): P<val> I<val> D<val>  e.g. P28.5"));
  Serial.println(F("========================="));
}

void Diagnostics::logEvent(const __FlashStringHelper* msg, uint32_t nowMs) {
  if (!DEBUG_ENABLED) return;
  // Rate-limit event logging so it cannot flood the time-critical loop.
  if (nowMs - lastEventMs_ < 100) return;
  lastEventMs_ = nowMs;
  Serial.print(F("[EVENT ")); Serial.print(nowMs); Serial.print(F("] "));
  Serial.println(msg);
}
