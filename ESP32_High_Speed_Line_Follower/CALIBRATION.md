# Sensor Calibration Guide

Accurate calibration is the single biggest factor in reliable line detection.
The QTR-8RC needs a per-channel min/max so each sensor maps its reflectance to a
clean 0–1000 calibrated value regardless of LED/photodiode variation.

---

## 1. What calibration does

For every channel the firmware records:
- `minimum` — reading over the brightest surface it saw (white)
- `maximum` — reading over the darkest surface it saw (black line)

Calibrated reads then scale each raw value into **0–1000** (1000 = fully on the
line, for a black line on white). The weighted average of these gives the line
**position (0–7000, center 3500)**, and their **sum** gives the **confidence**
used to tell a real detection from a retained last-known position.

## 2. Procedure (automatic on boot)

1. Power the ESP32. It prints the banner and enters **CALIBRATING**.
2. For ~**4.5 s** (`CALIBRATION_TIME_MS`), slowly **sweep the whole array back
   and forth across the black line and the white surface**, so every one of the
   8 sensors passes over both black and white.
3. The firmware samples every `CALIBRATION_STEP_MS` (20 ms).
4. On completion it validates contrast per channel:
   - Each channel must reach at least `SENSOR_CONTRAST_MIN` (default 300 counts)
     of (max − min).
   - Pass → **READY** (`[CAL] OK`).
   - Fail → **ERROR** with a `mask` showing which channels were weak
     (`[CAL] FAILED. Weak channels mask=0x..`).
5. Motors stay **inhibited** the whole time — calibration never moves the robot.

Recalibrate any time with the **button** (from a stopped state) or the Serial
command **`c`**.

## 3. Verifying calibration

Send **`d`** (diagnostics) after calibration and inspect the `cal=[...]` row
while holding the array:

- **Over white:** all channels should read **low** (≈ 0–150).
- **Over the black line:** the channel(s) on the line should read **high**
  (≈ 850–1000); off-line channels stay low.
- **Confidence** (`conf`) should be **high** when centered on the line and
  **near zero** when lifted off the track.
- `line=1` on the line, `line=0` when lifted.

If a channel never rises over black or never falls over white, that sensor is
dirty, misaligned, too high/low off the surface, or failing.

## 4. Tunable calibration parameters (`config.h`)

| Parameter | Default | Purpose |
|-----------|---------|---------|
| `CALIBRATION_TIME_MS` | 4500 | Length of the sweep window |
| `CALIBRATION_STEP_MS` | 20 | Sampling interval during the sweep |
| `SENSOR_CONTRAST_MIN` | 300 | Min (max−min) per channel to pass |
| `LINE_CONFIDENCE_MIN` | 350 | Summed calibrated value that counts as "line present" |
| `SENSOR_ON_LINE_MIN` | 200 | Per-channel value to count as "on the line" |
| `SENSOR_RC_TIMEOUT_US` | 2500 | RC discharge timeout (speed vs. sensitivity) |
| `LINE_LOST_DEBOUNCE` | 3 | Cycles absent before declaring loss |
| `LINE_FOUND_DEBOUNCE` | 3 | Cycles present before confirming reacquire |

### Choosing `LINE_CONFIDENCE_MIN`
Run `d` with the robot **on** the line and **off** the line; note the `conf`
values. Set the threshold roughly midway (closer to the off-line value) so lift
reliably reads as loss but a thin line still reads as present. This is the key
knob distinguishing a true reading from QTR's retained last-known position.

## 5. Important: retained position vs. real detection

Calibrated QTR position routines (and this firmware's held `getPosition()`)
return the **last valid position** when the line disappears, so position alone
cannot tell you the line is gone. **Always gate on `isLineDetected()` /
`getConfidence()`.** The firmware already does this — if you add features that
read position directly, keep the same rule.

## 6. Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `[CAL] FAILED` on some channels | sensors never saw black or white | sweep slower/wider; ensure every sensor crosses the line |
| All channels weak | array too high off surface | lower the array to ~3–8 mm; confirm emitters on |
| `conf` high even when lifted | threshold too low / ambient IR | raise `LINE_CONFIDENCE_MIN`; shield from direct sunlight |
| Noisy `pos` on a clean line | reflective track / stray light | clean surface; increase `LINE_FOUND_DEBOUNCE` |
| Calibration seems to drift over reruns | QTR accumulates min/max across calibrate() calls | power-cycle before a fresh calibration, or add `resetCalibration()` if your QTRSensors version provides it |
