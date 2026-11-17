# Test & Validation Plan

Run these in order, from first power-on to full-speed trials. **Keep the wheels
off the ground until Test 4 passes.** Do not claim a test passed unless you
actually performed it on hardware — record results in `CHANGELOG.md`.

Each test lists: **Objective · Setup · Procedure · Expected · Pass/Fail ·
Troubleshooting.**

---

## Test 1 — Hardware initialization & startup safety
- **Objective:** Firmware boots, GPIO/driver configured, motors inhibited.
- **Setup:** Logic power only (USB), motor power OFF. Serial Monitor @ 115200.
- **Procedure:** Reset the board; read the boot banner and command help.
- **Expected:** Banner prints; state enters `CALIBRATING`; **no motor motion**;
  with motor power applied and wheels raised, wheels stay still at boot.
- **Pass/Fail:** PASS if motors never move on boot and banner appears.
- **Troubleshooting:** No Serial → baud/port. Motors twitch at boot → check
  `STBY` wiring and that `begin()` ran before enable.

## Test 2 — Individual sensor detection
- **Objective:** All 8 channels respond to black and white.
- **Setup:** READY/after calibration; send `d`.
- **Procedure:** Pass a black card under each sensor one at a time; watch
  `cal=[...]`.
- **Expected:** The covered channel rises toward ~1000, others stay low.
- **Pass/Fail:** PASS if every channel responds distinctly.
- **Troubleshooting:** Dead channel → wiring/pin in `config.h`, sensor height,
  dirt on emitter/receiver.

## Test 3 — Calibration
- **Objective:** Each channel achieves adequate, stable contrast.
- **Setup:** Power on; perform the 4.5 s sweep (see `CALIBRATION.md`).
- **Procedure:** Sweep array over black+white; observe pass/fail message; `d`.
- **Expected:** `[CAL] OK`; over white all low, over black on-line channels high;
  `conf` high on line, ~0 off line.
- **Pass/Fail:** PASS if calibration validates and contrast check is met.
- **Troubleshooting:** `[CAL] FAILED mask` → which channels; sweep wider/slower,
  adjust array height, `SENSOR_CONTRAST_MIN`.

## Test 4 — Motor direction (wheels raised)
- **Objective:** Forward/reverse and steering sense are correct.
- **Setup:** **Wheels off the ground.** Motor power ON.
- **Procedure:** From READY, start (`s`). Nudge the array off-center left, then
  right, by hand and watch wheel response; or temporarily set
  `PWM_START=PWM_MAX=90` for a gentle test.
- **Expected:** Both wheels drive **forward** for "go". Line to the **left**
  (pos < 3500) → robot steers left (right wheel faster). Line right → steers
  right.
- **Pass/Fail:** PASS if forward is forward and steering turns toward the line.
- **Troubleshooting:** A wheel spins backward → flip `MOTOR_L_INVERT` /
  `MOTOR_R_INVERT`. Steers *away* from the line → set `STEERING_SIGN = -1`.
  **Fix in `config.h`, not by rewiring.**

## Test 5 — PID tracking (low speed)
- **Objective:** Correction drives the robot **toward** the line.
- **Setup:** On the line, `PWM_MAX` low (~90).
- **Procedure:** Start; let it follow a straight + gentle curve.
- **Expected:** Tracks the line; small corrections; no runaway.
- **Pass/Fail:** PASS if it stays on a gentle course without diverging.
- **Troubleshooting:** Diverges immediately → wrong `STEERING_SIGN`. Heavy
  wobble → see `PID_TUNING.md` steps 2–5.

## Test 6 — Straight-line speed
- **Objective:** Stable acceleration toward `PWM_MAX` (200) with low oscillation.
- **Setup:** Long straight; `PWM_MAX=200`.
- **Procedure:** Start on the straight; watch `base` climb and `sev≈0` in `d`.
- **Expected:** Smooth acceleration, minimal weave.
- **Pass/Fail:** PASS if it reaches high base speed without oscillating off.
- **Troubleshooting:** Weaves at speed → ↓`PWM_MAX` or ↑`Kd`; check missed
  deadlines.

## Test 7 — Curve handling
- **Objective:** Handles wide, moderate, tight, and S-curves.
- **Setup:** Track sections of each type.
- **Procedure:** Run each; observe speed reduction and tracking; A/B
  `ENABLE_PREDICTIVE`.
- **Expected:** Slows appropriately entering tight turns; no overshoot; resumes
  speed on exit.
- **Pass/Fail:** PASS if it completes all curve types without losing the line.
- **Troubleshooting:** Overshoots → ↓`PWM_CURVE`, ↑`Kd`, ↑`PID_CORRECTION_MAX`.

## Test 8 — Line loss
- **Objective:** Detects loss and performs a controlled search.
- **Setup:** Running; create a short gap or sweep the robot off the line.
- **Procedure:** Remove the line briefly; observe `RECOVERING` behavior.
- **Expected:** Enters `RECOVERING`, sweeps toward last-known side at
  `PWM_RECOVERY`, reacquires, returns to `RUNNING` without a derivative kick.
- **Pass/Fail:** PASS if it recovers on a reacquirable gap without wild spinning.
- **Troubleshooting:** Spins wildly → ↓`RECOVERY_TURN_RATIO`, ↓`PWM_RECOVERY`.
  Wrong direction → check `getLastDirection` / `SENSOR_FLIP`.

## Test 9 — Recovery timeout
- **Objective:** Motors stop safely if the line isn't found.
- **Setup:** Running; remove the line permanently.
- **Procedure:** Lift the robot off the line and keep it off.
- **Expected:** After `RECOVERY_TIMEOUT_MS` (1.2 s) → `STOPPED`, motors off,
  event logged; requires explicit restart.
- **Pass/Fail:** PASS if it stops within ~1.2 s and does not auto-restart.
- **Troubleshooting:** Doesn't stop → confirm `RECOVERY_TIMEOUT_MS` and that
  `TIMED_OUT` routes to `enterStopped`.

## Test 10 — Timing performance
- **Objective:** Measure real loop timing and missed 5 ms deadlines.
- **Setup:** Running on a representative lap; send `d` periodically.
- **Procedure:** Record `loop period`, `compute`, `sensor read`, `missed dl`.
- **Expected:** Period ≈ 5000 µs; compute well under budget; few/no missed
  deadlines.
- **Pass/Fail:** PASS if deadlines are consistently met (or document the real
  limit honestly).
- **Troubleshooting:** Many misses → ↓`SENSOR_RC_TIMEOUT_US`, consider QTR-8A, or
  relax `CONTROL_PERIOD_US` and re-tune (see `PID_TUNING.md` §5).

## Test 11 — Endurance
- **Objective:** Stability over many laps.
- **Setup:** Full track, fresh battery.
- **Procedure:** Run repeated laps; monitor heat, battery sag, consistency,
  line-loss/recovery counts (`d`).
- **Expected:** Consistent behavior; no thermal shutdown; recoveries bounded.
- **Pass/Fail:** PASS if it completes the planned laps without instability.
- **Troubleshooting:** Degrades as battery drops → the confidence cap helps, but
  re-tune near end-of-charge; motors hot → check current limits (`WIRING.md`).

## Test 12 — Speed optimization (timed trials)
- **Objective:** Compare settings under consistent conditions.
- **Setup:** Same track, same battery charge window, same start point.
- **Procedure:** For each config (gains, `PWM_MAX`, predictive on/off), run ≥3
  timed laps; record lap time **and** line-loss count. Change one variable at a
  time.
- **Expected:** A setting that lowers lap time **without** raising DNF/loss risk.
- **Pass/Fail:** Choose the fastest config that still finishes every trial.
- **Troubleshooting:** Faster but loses the line → you've passed the reliability
  limit; back off.

---

### Results log template

| Date | Test | Config (Kp/Ki/Kd, PWM_MAX, pred) | Result | Lap time | Losses | Notes |
|------|------|----------------------------------|--------|----------|--------|-------|
|      |      |                                  |        |          |        |       |
