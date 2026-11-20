# mBot2 Line Follower (CyberPi + Quad RGB Sensor)

A PID line follower for the **Makeblock mBot2**, with **on-screen Start/Stop and
live PID tuning** on the CyberPi — no laptop needed once it's uploaded.

Same control ideas as the ESP32 build (PID steering from a line error, a base
cruising speed, bounded line-loss recovery), but much simpler to wire: the mBot2
is an integrated robot and the **Quad RGB Sensor gives the line offset directly**.

> **Build status:** written against mBlock 5's mBot2 Python API but **not run on
> hardware here**. Expect to tweak one or two things on your firmware version
> (see *API variants* below). Test with the wheels off the ground first.

---

## 1. Hardware
- **mBot2** = CyberPi controller + mBot2 shield + 2 encoder (EM) motors
- **Quad RGB Line-Follower Sensor** plugged into one of the mBot2 shield ports
  (the sensor the code reads via `quad_rgb_sensor`)
- A track: a **black line on a light surface** (the sensor can also learn other
  colors)

## 2. Software / upload
1. Install **mBlock 5** (desktop) and connect the CyberPi by USB (or Bluetooth).
2. Switch mBlock to **Python mode** (not blocks), add the **mBot2** device.
3. Open / paste **`mbot2_line_follower.py`**.
4. Click **Upload** to put it on the CyberPi so it runs standalone (or **Run** to
   run it live while tethered).

## 3. Calibrate the sensor (once)
The Quad RGB Sensor learns the line vs. background itself — you don't calibrate
in code:
- Put the robot on the track so the sensor straddles the line, then **press the
  small button on the Quad RGB Sensor** to run its learn/calibration routine
  (follow its LED feedback). Do this whenever the track color/lighting changes.
- mBlock also has a sensor calibration panel if you prefer.

## 4. On-screen controls (CyberPi)
| Control | Action |
|---|---|
| **Button A** | **Start** following |
| **Button B** | **Stop** (back to the menu) |
| **Joystick ←/→** | Select which value to tune (Kp, Ki, Kd, Base) |
| **Joystick ↑/↓** | Increase / decrease the selected value |
| **Joystick middle** | Reset the integral term (useful mid-tuning) |

The screen shows the state (RUNNING/STOPPED) and the live Kp / Ki / Kd / Base,
with `>` marking the selected one.

## 5. Tuning (same method as before)
Make it **finish reliably before fast**.
1. `Ki = 0`, `Kd = 0`, low **Base** (e.g. 30). Raise **Kp** until it tracks with a
   little wobble.
2. Back `Kp` off ~15 %, then add **Kd** to damp the wobble.
3. Raise **Base** for speed; nudge `Kp`/`Kd` to keep it stable in curves.
4. Leave `Ki = 0` unless it rides consistently to one side.

Defaults in the file: `Kp 0.80, Ki 0.00, Kd 8.00, Base 35`. These are starting
guesses — the offset scale depends on your sensor, so expect to adjust. Copy your
good values back into `params` at the top of the file so they persist across boots.

If it steers **away** from the line, set `STEER_SIGN = -1` at the top.

## 6. How it works (code map)
- `quad_rgb_sensor.get_offset_track()` → the **error** (line position vs. center).
- `quad_rgb_sensor.get_line_station()` → which of the 4 probes see the line;
  `0` = line lost → it searches toward the last-seen side, then auto-stops after
  `LOST_TIMEOUT`.
- PID → `corr`; wheels driven differentially with `mbot2.drive_speed(left, right)`
  (RPM), clamped to `MAX_SPEED`.
- The CyberPi screen + buttons/joystick are the whole UI (`draw()` / `handle_input()`).

## 7. API variants (if something errors on upload)
Firmware/mBlock versions differ slightly. If a name isn't found, try:
- Import: `from mbuild import quad_rgb_sensor` **or** `import mbuild` then call
  `mbuild.quad_rgb_sensor.get_offset_track(1)`.
- Offset method: `get_offset_track(1)` (some call it `get_offset(1)`).
- Line probes: `get_line_station(1)` (some: `get_line_sta(1)`).
- Drive: `mbot2.drive_speed(l, r)` (continuous). Stop: `mbot2.drive_speed(0, 0)`
  or `mbot2.EM_stop("all")`.
- Buttons/joystick: `cyberpi.controller.is_press("a"/"b"/"up"/"down"/"left"/"right"/"middle")`.

Tell me your mBlock version if the names differ and I'll match them exactly.

## 8. Safety
- First run with the **wheels off the ground** to confirm steering direction.
- `Button B` stops immediately; it also auto-stops when the line is lost past the
  timeout.
