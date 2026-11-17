# ESP32 High-Speed Autonomous Line-Following Racing Robot

Competition-ready, modular Arduino-ESP32 firmware for an 8-sensor differential-drive
line follower. Deterministic 200 Hz PID steering, adaptive speed control, bounded
line-loss recovery, and a safety-first state machine.

> **Build status: UNVERIFIED.** This firmware was authored but **not compiled or
> run on hardware** in the environment that produced it. A successful compile is
> *not* proof of race-readiness — physical testing and tuning (see
> [`TEST_PLAN.md`](TEST_PLAN.md) and [`PID_TUNING.md`](PID_TUNING.md)) are
> mandatory. Verify the build against the exact library/board versions below.

---

## 1. Assumed hardware (confirm before final electrical spec)

| Subsystem        | Assumption                                   | Status |
|------------------|----------------------------------------------|--------|
| MCU              | ESP32 DevKit (classic ESP32), 240 MHz        | Fixed  |
| Sensor array     | Pololu QTR-8RC (8× RC reflectance)           | Assumed |
| Motor driver     | Toshiba TB6612FNG dual DC driver             | Assumed |
| Motors           | 2× brushed DC geared, differential drive     | Unspecified — RPM, gear ratio, wheel Ø, voltage all configurable |
| Battery          | Rechargeable pack sized for the motors       | Unspecified — chemistry/voltage/regulator **must be confirmed** |
| Button           | Momentary, to GND, external 10 kΩ pull-up    | Fixed (GPIO34 is input-only) |

Values marked `CONFIRM` in [`config.h`](config.h) are safe placeholders for
unspecified hardware and must be verified.

## 2. Firmware architecture

A single deterministic control loop runs at **5 ms (200 Hz)**, scheduled with
`micros()` (no blocking `delay()` while racing). Each driving cycle performs:

1. Sensor acquisition (QTR-8RC calibrated read)
2. Line-confidence estimation + line-loss detection
3. Normalized-error calculation `error = (position − 3500) / 3500`
4. PID steering correction (filtered derivative, anti-windup, saturation)
5. Adaptive base-speed calculation (straight/curve/predictive)
6. Differential motor command `L = base + corr`, `R = base − corr`
7. Safety checks + saturation
8. Motor output with acceleration (slew) limiting

### Module map

| File | Responsibility |
|------|----------------|
| `ESP32_High_Speed_Line_Follower.ino` | Main app, scheduler, FSM, UI |
| `config.h`              | **All** pins, polarity, timing, tuning (single source of truth) |
| `sensors.{h,cpp}`       | Calibration, acquisition, weighted position, confidence, line-loss |
| `pid_controller.{h,cpp}`| Normalized-error PID (P/I/D, filter, anti-windup, saturation) |
| `motor_control.{h,cpp}` | TB6612 interface, LEDC PWM, direction, slew limit, e-stop |
| `speed_controller.{h,cpp}` | Adaptive speed, curve severity, predictive slow-down |
| `line_recovery.{h,cpp}` | Bounded search sweep + reacquire + timeout |
| `race_manager.{h,cpp}`  | FSM state, race timing, loop-timing stats, counters |
| `diagnostics.{h,cpp}`   | Throttled Serial telemetry + command dumps |

### State machine

```
BOOT → CALIBRATING → READY → RUNNING ⇄ RECOVERING
                         ↑        │           │
                         │        └── FINISHED (optional)
                         └──── STOPPED / ERROR ──┘
```

- Motors are **inhibited** at boot and in every non-driving state.
- Racing starts **only** on an explicit command (button in `READY`, or `s`).
- A reboot never auto-starts the motors.

## 3. Libraries, board & IDE setup

### Required
- **Arduino-ESP32 core 3.x** (uses the LEDC v3 API: `ledcAttach`/`ledcWrite`).
- **QTRSensors** library by Pololu (maintained; current API).

### Arduino IDE
1. **Boards Manager → `esp32` by Espressif → install a 3.x version** (e.g. 3.0.x
   or newer). This firmware uses `ledcAttach(pin, freq, resolution)`, which does
   **not** exist on 2.x cores.
2. **Library Manager → search "QTRSensors" (Pololu) → Install.**
3. Select board: **ESP32 Dev Module** (or your exact DevKit).
   - Upload speed: 921600 (drop to 115200 if uploads fail)
   - Flash frequency: 80 MHz, CPU: 240 MHz
4. Open `ESP32_High_Speed_Line_Follower.ino` (keep all files in the same folder —
   the `.ino` sketch folder must contain every `.h/.cpp`).
5. Select the serial port, **Upload**, then open **Serial Monitor @ 115200**.

> If you must stay on core **2.x**, the LEDC calls in `motor_control.cpp` need the
> old `ledcSetup()` + `ledcAttachPin()` form. Supported, documented combination
> for this release: **core 3.x + current QTRSensors**.

### PlatformIO
Use the included [`platformio.ini`](platformio.ini): `pio run -t upload` then
`pio device monitor`.

## 4. First run

1. Power the logic (USB ok for bench) **before** motor power for bench tests.
2. On reset the robot enters **CALIBRATING** and prints a prompt.
3. Sweep the sensor array across the black line and white surface for ~4.5 s
   (see [`CALIBRATION.md`](CALIBRATION.md)).
4. On success it enters **READY**. On weak contrast it enters **ERROR** — send
   `c` to recalibrate.
5. Place the robot on the line and press the button (or send `s`) to race.
6. Press the button again, or send `x`, to stop.

## 5. Serial commands (115200 baud)

| Cmd | Action |
|-----|--------|
| `s` | Start race (only from READY) |
| `x` | Stop immediately |
| `c` | Recalibrate |
| `p` | Print PID parameters |
| `d` | Print diagnostics (timing, sensors, motors) |
| `h` | Help |
| `P<v>` `I<v>` `D<v>` | Set a PID gain at runtime, e.g. `P28.5` + Enter (validated) |

For competition, define `COMPETITION_MODE_ON` (see `config.h`) to silence
telemetry in the hot path.

## 6. Key tunables (edit `config.h`)

| Parameter | Default | Meaning |
|-----------|---------|---------|
| `PWM_START` | 100 | Launch base PWM |
| `PWM_MAX` | 200 | Max racing base PWM |
| `PWM_CURVE` | 100 | Base PWM in sharp curves |
| `PWM_RECOVERY` | 85 | Search speed |
| `CONTROL_PERIOD_US` | 5000 | 5 ms = 200 Hz control loop |
| `RECOVERY_TIMEOUT_MS` | 1200 | Hard stop if line not reacquired |
| `PID_KP / KI / KD` | 30 / 0 / 0.30 | Provisional normalized-error gains |
| `STEERING_SIGN` | +1 | Flip to −1 if steering is inverted |
| `ENABLE_PREDICTIVE` | true | Predictive cornering on/off |
| `ENABLE_FINISH_LINE` | false | Finish detection (define geometry first) |

## 7. Documentation index

- [`WIRING.md`](WIRING.md) — pin table, electrical precautions
- [`CALIBRATION.md`](CALIBRATION.md) — calibration procedure & verification
- [`PID_TUNING.md`](PID_TUNING.md) — progressive tuning guide
- [`TEST_PLAN.md`](TEST_PLAN.md) — power-on → full-speed validation
- [`CHANGELOG.md`](CHANGELOG.md) — development history

## 8. Safety

Firmware safeguards (startup inhibit, PWM saturation, bounded recovery, safe
stop) are **not** a substitute for a **physical emergency power disconnect**.
Always provide a hardware kill switch on the motor supply. See `WIRING.md`.
