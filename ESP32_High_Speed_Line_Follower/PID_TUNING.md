# PID Tuning & Competition Optimization

The controller uses **normalized error** (`error = (position − 3500) / 3500`,
range ≈ −1…+1). **All gains in `config.h` are for this normalized error.** Do
not paste in gains tuned for raw 0–7000 error — they will be ~3500× too large.

Provisional starting point (in `config.h`):

```
Kp = 30.0    Ki = 0.0    Kd = 0.30
```

These are a *starting* point, not optimal values. Tune on your track, your
robot, your battery state.

> **Golden rule: make it finish reliably before you make it fast.** A robot that
> completes every lap at moderate speed beats one that's fast but DNFs.

---

## 1. Runtime tuning workflow

You can change gains live over Serial without reflashing:

- `p` — print current `Kp/Ki/Kd` and the latest P/I/D terms
- `P<v>` / `I<v>` / `D<v>` — set a gain (e.g. `P24` then Enter). Validated to
  `Kp∈[0,200]`, `Ki∈[0,50]`, `Kd∈[0,50]`.
- `d` — diagnostics (position, error, correction, loop timing, missed deadlines)

Runtime changes are **not** persisted — copy good values back into `config.h`.

## 2. Progressive tuning procedure

Do these in order. Keep speed low until step 6.

1. **Low-speed P only.** Set `Ki=0`, `Kd=0`, `PWM_START=PWM_MAX=90` (slow).
   Place on the line and start. Robot should track, likely wobbling.
2. **Raise `Kp`** until the robot follows gentle curves and corrects toward the
   line briskly. Too low → sluggish, cuts corners, drifts off. Too high →
   fast side-to-side oscillation.
3. **Back `Kp` off** ~10–20 % below the point where sustained oscillation
   starts.
4. **Add `Kd`** to damp the remaining oscillation. Start at `Kd=0.2` and
   increase. `Kd` resists *rate of change* of error — it calms wobble and
   sharpens corner entry. Too high → twitchy/jittery on noise.
5. **Noise filtering.** If `Kd` makes the robot jittery on a clean line, raise
   `PID_D_FILTER` (0.70 → 0.80) to smooth the derivative, and/or increase
   `LINE_FOUND_DEBOUNCE`. Re-check `Kd`.
6. **Straight-line speed.** Raise `PWM_MAX` toward 200. Confirm it accelerates on
   straights (`sev` near 0 in telemetry) without oscillating. If it weaves at
   speed, lower `PWM_MAX` or raise `Kd` slightly.
7. **Sharp-curve speed.** Tune `PWM_CURVE`, `ERR_SHARP_MIN`, and `DERR_SHARP`
   so the robot slows *entering* tight turns and doesn't overshoot. Lower
   `PWM_CURVE` if it flies wide; raise it if it's needlessly slow.
8. **Steering authority limits.** If corrections slam the motors into reversal,
   lower `PID_CORRECTION_MAX`; if it can't turn hard enough in hairpins, raise
   it (up to 255).
9. **Recovery.** Tune `PWM_RECOVERY` and `RECOVERY_TURN_RATIO` so a lost line is
   found by a controlled sweep, not a wild spin. Confirm it stops cleanly at
   `RECOVERY_TIMEOUT_MS`.
10. **Full-lap optimization.** Time laps (`d` shows race elapsed). Adjust
    `BASE_ACCEL_STEP`/`BASE_DECEL_STEP` for smooth corner exit acceleration.
    Only now push `PWM_MAX` to the edge of reliability.

**`Ki` is usually left at 0** for line followers — the line is a position
reference with little steady-state offset, and integral invites windup/overshoot.
Add a *tiny* `Ki` (e.g. 0.5–2) only if the robot consistently rides slightly to
one side on long arcs; the anti-windup clamp (`PID_I_LIMIT`) is already in place.

## 3. Symptom → cause → fix

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Fast left/right oscillation | `Kp` too high / `Kd` too low | ↓`Kp`, ↑`Kd` |
| Slow, lazy, cuts corners | `Kp` too low | ↑`Kp` |
| Delayed/late steering | `Kp` low or loop timing overruns | ↑`Kp`; check `d` for missed deadlines |
| Overshoots line after correction | `Kd` too low or speed too high | ↑`Kd`; ↓`PWM_MAX`/`PWM_CURVE` |
| Twitchy/jittery on clean line | derivative amplifying noise | ↑`PID_D_FILTER`, ↑debounce, ↓`Kd` |
| Sudden hard braking | curve thresholds too sensitive | ↑`ERR_SHARP_MIN`, ↑`DERR_SHARP`, ↑`BASE_DECEL_STEP` smoothing |
| Unwanted motor reversals | correction unsaturated | ↓`PID_CORRECTION_MAX` |
| Poor corner-exit acceleration | base speed ramps up too slowly | ↑`BASE_ACCEL_STEP` |
| Steers the wrong way (away from line) | inverted steering sign | set `STEERING_SIGN = -1` (and verify Test 5) |
| Flies off on hairpins | too fast into the turn / authority capped | ↓`PWM_CURVE`, ↑`PID_CORRECTION_MAX`, enable predictive |
| Never recovers after loss | search direction/speed wrong | check `getLastDirection`; tune `PWM_RECOVERY`, `RECOVERY_TURN_RATIO` |
| Sensor readings unstable | calibration/ambient light | recalibrate; see `CALIBRATION.md` |
| Loop timing overruns (`miss>0`) | sensor read too slow | ↓`SENSOR_RC_TIMEOUT_US`; see timing note below |

## 4. Predictive cornering

With `ENABLE_PREDICTIVE = true`, the speed controller watches a short history of
error magnitude and pre-emptively trims base speed when the trend shows the turn
*sharpening* — using only measured data (it cannot see beyond the sensor's field
of view). To A/B test, set `ENABLE_PREDICTIVE = false`, run timed laps, then
`true`, and compare lap time **and** line-loss count (`d`). Tune
`PREDICT_SLOWDOWN` and `PREDICT_HISTORY` for your track.

## 5. Loop-timing note (5 ms target)

At 200 Hz, an 8-channel **RC** read can take up to `SENSOR_RC_TIMEOUT_US`
(2500 µs) in the worst case (very dark), which is a large fraction of the 5 ms
budget. Watch `d` → `loopUs`, `compute`, `sensor read`, and `missed dl`. If
missed deadlines accumulate:
- Lower `SENSOR_RC_TIMEOUT_US` (e.g. 1500 µs) — reduces worst-case read time at
  some sensitivity cost.
- Consider a **QTR-8A (analog)** array (ADC reads are faster/bounded) via the
  modular sensor interface.
- Or relax the control period (e.g. 6–8 ms) and re-tune.

Report the real limitation from telemetry rather than assuming 5 ms is met —
the firmware measures it so you can decide honestly.
