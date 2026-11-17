# Changelog

All notable changes to this firmware are documented here.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

## [0.1.0] — 2026-11-17 — Initial firmware

### Added
- Modular Arduino-ESP32 (core 3.x) project for an 8-sensor, differential-drive
  line-following racing robot.
- **Deterministic 5 ms / 200 Hz control loop** scheduled with `micros()`,
  rollover-safe, non-blocking (no `delay()` while racing).
- **Finite-state machine:** BOOT → CALIBRATING → READY → RUNNING ⇄ RECOVERING →
  STOPPED/ERROR, plus optional FINISHED. Motors inhibited at boot and in all
  non-driving states; no auto-start on reboot.
- **Sensor module** (`sensors.*`): QTR-8RC via QTRSensors, non-blocking
  calibration window (~4.5 s), weighted 0–7000 position, summed-confidence
  line-loss detection with debounce, last-known position/direction retention,
  modular interface for future QTR-8A/comparator arrays.
- **PID module** (`pid_controller.*`): normalized-error PID, measured-dt
  compensation, EMA-filtered derivative, bounded integral with anti-windup,
  correction saturation, derivative-kick priming, runtime gain adjustment.
- **Motor module** (`motor_control.*`): TB6612FNG via LEDC v3 API
  (`ledcAttach`/`ledcWrite`), signed speeds, polarity inversion, L/R trim,
  accel/decel slew limiting, saturation, standby handling, emergency brake-stop.
- **Adaptive speed** (`speed_controller.*`): straight/curve classification from
  error + its filtered rate, confidence-based speed cap, ramped base-speed
  changes, optional predictive slow-down from a short error history.
- **Line recovery** (`line_recovery.*`): bounded controlled search toward the
  last-known side, multi-sample reacquire, hard 1.2 s timeout → safe stop.
- **Race manager** (`race_manager.*`): state model, race timing, loop-timing
  statistics (period/compute/missed deadlines), line-loss/recovery counters.
- **Diagnostics** (`diagnostics.*`): throttled one-line telemetry, PID/diag
  dumps, rate-limited event log, help.
- **UI:** debounced start/stop button (GPIO34, external pull-up) and Serial
  commands `s/x/c/p/d/h` plus validated runtime tuning `P/I/D<value>`.
- **Config** (`config.h`): single source of truth for pins, polarity, PWM
  freq/resolution, timing, PID, speed profile, recovery, calibration, feature
  flags; unspecified hardware marked `CONFIRM`.
- Optional, disabled-by-default stubs: finish-line detection, battery monitor,
  encoder interface.
- Documentation: `README`, `WIRING`, `CALIBRATION`, `PID_TUNING`, `TEST_PLAN`,
  and `platformio.ini`.

### Known limitations / to confirm
- **Build is UNVERIFIED** — not compiled or hardware-tested in the authoring
  environment. Compile against Arduino-ESP32 **3.x** + current **QTRSensors**.
- Worst-case 8-channel RC read (`SENSOR_RC_TIMEOUT_US`) can consume a large part
  of the 5 ms budget; the firmware *measures* loop timing so the real limit can
  be reported (see `PID_TUNING.md` §5). Reduce timeout or move to QTR-8A if
  deadlines are missed.
- Motor RPM, gear ratio, wheel diameter, motor voltage, battery chemistry and
  regulator design are **unspecified** and must be confirmed before a final
  electrical spec.
- Finish-line, battery monitoring and encoders are interface stubs only.
- Default tuning (Kp=30, Ki=0, Kd=0.30) and speed/curve thresholds are
  **provisional** starting points requiring on-track tuning.

### Next
- See `README` §Future / `PID_TUNING` — encoder feedback & speed matching,
  live data logging, finish-marker geometry + detection, intersection handling.
